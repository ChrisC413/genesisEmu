// Machine save states (spec 08 "Save states", M06-00a): which chunk holds which component, and the
// load sequence (validate everything, apply, verify the recorded state hash, roll back on failure).

#include <algorithm>
#include <array>

#include "core/sched/machine.hpp"
#include "core/state/archive.hpp"
#include "core/state/save_state.hpp"

namespace gen {

namespace {

struct ChunkDef {
    std::string_view tag;
    std::uint16_t version;  // the version this build writes and the newest it reads
};

// Every chunk a state must contain, in file order (INFO comes first and is handled separately).
constexpr std::array<ChunkDef, 9> kChunks{{
    {"M68K", 1},
    {"BUS ", 1},
    {"VDP ", 1},
    {"IO  ", 1},
    {"Z80 ", 1},
    {"Z80C", 1},
    {"SND ", 1},
    {"CART", 1},
    {"SCHD", 1},
}};

const ChunkDef* findChunk(std::string_view tag) {
    const auto it =
        std::find_if(kChunks.begin(), kChunks.end(), [&](const ChunkDef& d) { return d.tag == tag; });
    return it == kChunks.end() ? nullptr : &*it;
}

void serializeM68k(state::Archive& ar, m68k::M68k& cpu) {
    m68k::State& c = cpu.state();
    for (auto& r : c.d) {
        ar.u32(r);
    }
    for (auto& r : c.a) {
        ar.u32(r);
    }
    ar.u32(c.usp);
    ar.u32(c.ssp);
    ar.u32(c.pc);
    ar.u16(c.sr);
    ar.u16(c.ir);
    ar.u16(c.irc);
    ar.boolean(c.stopped);
    ar.boolean(c.halted);
    int level = cpu.irqLevel();
    bool nmi = cpu.nmiPending();
    ar.i32(level);
    ar.boolean(nmi);
    if (ar.loading()) {
        cpu.restoreIrq(level, nmi);
    }
}

void serializeIo(state::Archive& ar, bus::IoState& io) {
    ar.u8(io.version);
    ar.bytes(io.data);
    ar.bytes(io.control);
    ar.bytes(io.serial);
    ar.u16s(io.buttons);
    for (std::size_t p = 0; p < 2; ++p) {
        ar.enum8(io.padType[p]);
        bool th = io.th[p];
        ar.boolean(th);
        io.th[p] = th;
        ar.u8(io.thCount[p]);
        ar.u64(io.thChangedAt[p]);
    }
}

void serializeZ80Control(state::Archive& ar, z80::Z80Control& z80) {
    z80::Z80ControlState& z = z80.state();
    ar.bytes(z.ram);
    ar.boolean(z.busRequested);
    ar.boolean(z.resetAsserted);
    ar.u16(z.bank);
    ar.u8(z.ymAddress[0]);
    ar.u8(z.ymAddress[1]);
    ar.bytes(z.ymRegs[0]);
    ar.bytes(z.ymRegs[1]);
    ar.u8(z.psgLast);
    ar.u8(z.psgLatch);
    int stall = z80.pendingM68kStall();
    ar.i32(stall);
    if (ar.loading()) {
        z80.setPendingM68kStall(stall);
    }
}

void serializeZ80(state::Archive& ar, z80::Z80State& z) {
    for (std::uint8_t* r : {&z.a, &z.f, &z.b, &z.c, &z.d, &z.e, &z.h, &z.l, &z.i, &z.r, &z.im, &z.q}) {
        ar.u8(*r);
    }
    for (std::uint16_t* r : {&z.af2, &z.bc2, &z.de2, &z.hl2, &z.ix, &z.iy, &z.sp, &z.pc, &z.wz}) {
        ar.u16(*r);
    }
    for (bool* b : {&z.iff1, &z.iff2, &z.halted, &z.eiDelay, &z.ldAir, &z.irqLine, &z.nmiPending}) {
        ar.boolean(*b);
    }
}

}  // namespace

void Machine::serializeChunk(std::string_view tag, state::Archive& ar) {
    if (tag == "M68K") {
        serializeM68k(ar, *cpu_);
    } else if (tag == "BUS ") {
        bus::BusState& b = bus_->state();
        ar.bytes(b.ram);
        ar.u32(b.tmssRegister);
    } else if (tag == "VDP ") {
        vdp_.serialize(ar);
    } else if (tag == "IO  ") {
        serializeIo(ar, io_.state());
    } else if (tag == "Z80 ") {
        serializeZ80Control(ar, z80_);
    } else if (tag == "Z80C") {
        serializeZ80(ar, z80cpu_->state());
    } else if (tag == "SND ") {
        sound_.serialize(ar);
    } else if (tag == "CART") {
        // Each device as (name, payload) in chain order; the chain itself comes from the ROM header.
        auto count = static_cast<std::uint32_t>(devices_.size());
        ar.u32(count);
        if (ar.loading() && count != devices_.size()) {
            ar.fail("the cartridge has a different device chain");
            return;
        }
        for (const auto& d : devices_) {
            std::string name(d->name());
            std::vector<std::uint8_t> payload;
            if (!ar.loading()) {
                state::Archive sub(payload, ar.version());
                d->serialize(sub);
            }
            ar.string(name);
            ar.blob(payload);
            if (ar.loading() && ar.ok()) {
                if (name != d->name()) {
                    ar.fail("cartridge device '" + name + "' does not match '" + std::string(d->name()) +
                            "'");
                    return;
                }
                state::Archive sub(std::span<const std::uint8_t>(payload), ar.version());
                d->serialize(sub);
                if (!sub.ok() || !sub.atEnd()) {
                    ar.fail("cartridge device '" + name +
                            "': " + (sub.ok() ? "trailing bytes" : sub.error()));
                    return;
                }
            }
        }
    } else if (tag == "SCHD") {
        auto region = static_cast<std::uint8_t>(region_);
        ar.u8(region);
        if (ar.loading() && ar.ok() && region != static_cast<std::uint8_t>(region_)) {
            ar.fail(std::string("the state was saved with region ") +
                    bus::toString(static_cast<bus::Region>(region)) + ", this machine runs " +
                    bus::toString(region_));
            return;
        }
        ar.u64(frame_);
        ar.i32(line_);
        ar.u64(clock_.lineStart);
        ar.u64(clock_.cpuMclk);
        ar.u64(z80Mclk_);
        ar.u16s(pendingInput_);
    }
}

std::vector<std::uint8_t> Machine::saveState(std::uint64_t timestamp) {
    state::StateFile file;
    const state::StateInfo info{romSha1_, frame_, timestamp, stateHash()};
    file.chunks.push_back({"INFO", 1, state::encodeInfo(info)});
    for (const ChunkDef& def : kChunks) {
        state::StateChunk chunk{std::string(def.tag), def.version, {}};
        state::Archive ar(chunk.payload, def.version);
        serializeChunk(def.tag, ar);
        file.chunks.push_back(std::move(chunk));
    }
    return state::encodeStateFile(file);
}

std::string Machine::applyState(const state::StateFile& file) {
    for (const ChunkDef& def : kChunks) {
        const state::StateChunk* chunk = file.find(def.tag);
        state::Archive ar(std::span<const std::uint8_t>(chunk->payload), chunk->version);
        serializeChunk(def.tag, ar);
        if (!ar.ok() || !ar.atEnd()) {
            return "save state chunk '" + chunk->tag + "' is corrupt (" +
                   (ar.ok() ? "trailing bytes" : ar.error()) + ")";
        }
    }
    vdp_.invalidateCaches();
    z80_.log().setPosition(frame_, line_);
    return {};
}

bool Machine::loadState(std::span<const std::uint8_t> bytes, std::string& error) {
    const auto file = state::decodeStateFile(bytes, error);
    if (!file) {
        return false;
    }
    const state::StateChunk* infoChunk = file->find("INFO");
    if (infoChunk == nullptr) {
        error = "save state has no 'INFO' chunk";
        return false;
    }
    const auto info = state::decodeInfo(*infoChunk, error);
    if (!info) {
        return false;
    }
    if (info->romSha1 != romSha1_) {
        error = "the state was saved from another ROM (SHA-1 " + toHex(info->romSha1) + "); this ROM is " +
                toHex(romSha1_);
        return false;
    }
    for (const state::StateChunk& c : file->chunks) {
        if (c.tag == "INFO") {
            continue;
        }
        const ChunkDef* def = findChunk(c.tag);
        if (def == nullptr) {
            error = "save state chunk '" + c.tag +
                    "' is not understood by this build (saved by a newer version?)";
            return false;
        }
        if (c.version == 0 || c.version > def->version) {
            error = "save state chunk '" + c.tag + "' has version " + std::to_string(c.version) +
                    "; this build reads up to version " + std::to_string(def->version);
            return false;
        }
    }
    for (const ChunkDef& def : kChunks) {
        if (file->find(def.tag) == nullptr) {
            error = "save state has no '" + std::string(def.tag) + "' chunk";
            return false;
        }
    }
    // Apply on top of a backup so a failure leaves the machine as it was.
    const std::vector<std::uint8_t> backup = saveState();
    std::string failure = applyState(*file);
    if (failure.empty() && stateHash() != info->stateHash) {
        failure = "the loaded state does not reproduce the state hash recorded when it was saved";
    }
    if (failure.empty()) {
        return true;
    }
    std::string ignored;
    if (const auto restore = state::decodeStateFile(backup, ignored)) {
        applyState(*restore);
    }
    error = failure;
    return false;
}

}  // namespace gen
