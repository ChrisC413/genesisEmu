#include "core/sched/machine.hpp"

#include <algorithm>

#include "core/state/state_hash.hpp"

namespace gen {

namespace {
constexpr int kVintVector = 24 + 6;
}

const char* toString(StopReason reason) {
    switch (reason) {
        case StopReason::LimitReached:
            return "limit";
        case StopReason::Breakpoint:
            return "breakpoint";
        case StopReason::Watchpoint:
            return "watchpoint";
        case StopReason::Exception:
            return "exception";
        case StopReason::Unsupported:
            return "unsupported";
    }
    return "?";
}

bool isFaultVector(int vector) {
    return vector >= 2 && vector <= 24;
}

Machine::Machine(std::vector<std::uint8_t> rom, std::uint8_t headerRegions, const MachineConfig& config)
    : rom_(std::move(rom)), romSha1_(sha1(rom_)), config_(config) {
    // Pad to whole 64 KB pages so every ROM page can be a direct fast page.
    rom_.resize(std::min<std::size_t>((rom_.size() + 0xFFFF) & ~std::size_t{0xFFFF}, 0x400000), 0xFF);
    region_ = config.region.value_or(bus::regionFromHeader(headerRegions));
    pal_ = region_ == bus::Region::Europe;
    bus_ = std::make_unique<bus::SystemBus>(vdp_, io_, z80_, clock_);
    cpu_ = std::make_unique<m68k::M68k>(*bus_);
    bus_->attach(rom_, *cpu_, config_.tmss);
    z80cpu_ = std::make_unique<z80::Z80>(z80_);
    z80cpu_->setHooks(&hooks_);
    z80_.attach(z80cpu_.get(), bus_.get(), &hooks_);
    z80_.setWindowTiming(config_.z80Window);
    z80_.setSound(&sound_);
    sound_.setClockSource(&Machine::soundNow, this);
    bus_->setZ80Sync(&Machine::syncZ80, this);
    // SRAM from the header at $1B0; a lock-on image declares it in the locked-on cartridge's header.
    if (rom_.size() >= cart::kMinRomSize) {
        cart::SramDeclaration sram = cart::parseHeader(rom_).sram;
        if (!sram.declared && rom_.size() > 0x200000 + cart::kMinRomSize &&
            std::equal(rom_.begin() + 0x200100, rom_.begin() + 0x200104, "SEGA")) {
            sram = cart::parseHeader(std::span(rom_).subspan(0x200000)).sram;
        }
        if (sram.declared && sram.range.end >= sram.range.start) {
            auto device = std::make_unique<cart::SramDevice>(sram, rom_.size());
            sram_ = device.get();
            addCartDevice(std::move(device));
        }
    }
    vdp_.setHooks(&hooks_);
    hardReset();
}

Machine::~Machine() = default;

void Machine::hardReset() {
    clock_ = bus::BusClock{};
    frame_ = 0;
    line_ = 0;
    stats_ = MachineStats{};
    bus_->reset();
    bus_->clearIssues();
    io_.reset(region_, pal_, config_.tmss);
    io_.state().buttons = pendingInput_;
    for (const auto& d : devices_) {
        d->reset();
    }
    z80_.reset();
    z80_.log().clear();
    z80cpu_->state() = z80::Z80State{};
    z80cpu_->reset();
    z80Mclk_ = 0;
    vintAt_ = 0;
    sound_.reset(pal_);
    vdp_.reset(pal_);
    cpu_->state() = m68k::State{};
    cpu_->setIrqLevel(0);
    const int cycles = cpu_->reset();
    clock_.cpuMclk = static_cast<std::uint64_t>(cycles) * kM68kDivider;
    vdp_.beginLine(0);
    bus_->updateIrq();
}

void Machine::addCartDevice(std::unique_ptr<cart::ICartDevice> device) {
    devices_.push_back(std::move(device));
    std::vector<cart::ICartDevice*> chain;
    for (const auto& d : devices_) {
        chain.push_back(d.get());
    }
    bus_->setCartDevices(std::move(chain));
}

void Machine::setHooks(const Hooks& hooks) {
    hooks_ = hooks;
    cpu_->setHooks(hooks_);
}

void Machine::catchUp(std::uint64_t mclk) {
    runZ80(mclk);
    sound_.advanceTo(mclk);
}

std::uint64_t Machine::soundNow(void* ctx) {
    const auto& m = *static_cast<const Machine*>(ctx);
    return m.inZ80_ ? m.z80Mclk_ : m.clock_.cpuMclk;
}

void Machine::syncZ80(void* ctx) {
    auto& m = *static_cast<Machine*>(ctx);
    m.runZ80(m.clock_.cpuMclk);
}

void Machine::runZ80(std::uint64_t mclk) {
    if (inZ80_) {
        return;  // a Z80 access through the bank window reached the 68k bus
    }
    inZ80_ = true;
    z80cpu_->setIrq(vdp_.state().z80Int);
    while (z80Mclk_ < mclk) {
        if (!z80_.running()) {
            z80Mclk_ = mclk;  // held by BUSREQ or RESET: time passes without execution
            break;
        }
        const int t = z80cpu_->step();
        if (t == 0) {
            break;  // breakpoint (M06 surfaces Z80 breaks in run())
        }
        z80Mclk_ += static_cast<std::uint64_t>(t) * kZ80Divider;
    }
    // The 68k loses the cycles the Z80's window accesses held its bus.
    clock_.cpuMclk += static_cast<std::uint64_t>(z80_.takeM68kStall()) * kM68kDivider;
    inZ80_ = false;
}

void Machine::endLine() {
    const std::uint64_t lineEnd = clock_.lineStart + kMclkPerLine;
    const std::uint64_t beforeCatchUp = clock_.cpuMclk;
    catchUp(lineEnd);
    // Z80 bank-window stalls of the ending line are charged to the 68k here, in one lump, although on
    // hardware they happened during the line. They delay the 68k's view of this boundary (and so its H-int
    // entry); the V-int moves with it so the H-int -> V-int distance stays as on hardware (M06-00b).
    const std::uint64_t stallLump = clock_.cpuMclk - beforeCatchUp;
    vdp_.endLine(line_);
    clock_.lineStart = lineEnd;
    if (++line_ >= linesPerFrame()) {
        line_ = 0;
        ++frame_;
        stats_.vintsLastFrame = stats_.vintsThisFrame;
        stats_.vintsThisFrame = 0;
        io_.state().buttons = pendingInput_;
        if (hooks_.onFrameEnd != nullptr) {
            hooks_.onFrameEnd(hooks_.ctx, frame_ - 1);
        }
    }
    vdp_.beginLine(line_);
    if (vdp_.vintArmed()) {
        vintAt_ = clock_.lineStart + static_cast<std::uint64_t>(vdp_.vintDelayMclk()) + stallLump;
    }
    z80_.log().setPosition(frame_, line_);
    bus_->updateIrq();
}

void Machine::checkVint() {
    if (vdp_.vintArmed() && clock_.cpuMclk >= vintAt_) {
        vdp_.raiseVint();
        bus_->updateIrq();
    }
}

bool Machine::stepCpu(RunResult& result) {
    const std::uint32_t pc = cpu_->state().pc;
    const int cycles = cpu_->step();
    if (cpu_->breakHit()) {
        result.reason = StopReason::Breakpoint;
        return false;
    }
    ++result.instructions;
    clock_.cpuMclk += static_cast<std::uint64_t>(cycles) * kM68kDivider;
    const int vector = cpu_->lastException();
    if (vector >= 0) {
        ++stats_.exceptions[static_cast<std::size_t>(vector & 0xFF)];
        if (vector == kVintVector) {
            ++stats_.vints;
            ++stats_.vintsThisFrame;
            stats_.lastVintPc = pc;
        } else if (isFaultVector(vector)) {
            if (stats_.faults++ == 0) {
                stats_.firstFaultVector = vector;
                stats_.firstFaultPc = pc;
            }
            if (config_.stopOnFault) {
                result.reason = StopReason::Exception;
                result.vector = vector;
            }
        }
    }
    while (clock_.cpuMclk >= clock_.lineStart + kMclkPerLine) {
        endLine();
    }
    checkVint();
    // A 68k -> VDP DMA started by this instruction freezes the 68k until the transfer is done; the VDP
    // gets the time line by line at the DMA bandwidth of each line (M03-02).
    while (vdp_.dmaFreezesCpu()) {
        const auto avail = static_cast<int>(clock_.lineStart + kMclkPerLine - clock_.cpuMclk);
        clock_.cpuMclk += static_cast<std::uint64_t>(vdp_.runDma(avail));
        while (clock_.cpuMclk >= clock_.lineStart + kMclkPerLine) {
            endLine();
        }
        checkVint();
    }
    return result.reason != StopReason::Exception;
}

RunResult Machine::run(RunLimit limit) {
    RunResult result;
    const std::uint64_t targetFrame = frame_ + limit.count;
    const std::uint64_t targetMclk = clock_.cpuMclk + limit.count;
    for (;;) {
        switch (limit.kind) {
            case RunLimit::Kind::Frames:
                if (frame_ >= targetFrame) {
                    return result;
                }
                break;
            case RunLimit::Kind::Instructions:
                if (result.instructions >= limit.count) {
                    return result;
                }
                break;
            case RunLimit::Kind::Mclk:
                if (clock_.cpuMclk >= targetMclk) {
                    return result;
                }
                break;
            case RunLimit::Kind::UntilBreak:
                break;
        }
        if (!stepCpu(result)) {
            return result;
        }
    }
}

RunResult Machine::step(CpuId cpu) {
    RunResult result;
    if (cpu == CpuId::Z80) {
        if (!z80_.running()) {
            return result;
        }
        z80cpu_->setIrq(vdp_.state().z80Int);
        const int t = z80cpu_->step();
        if (z80cpu_->breakHit()) {
            result.reason = StopReason::Breakpoint;
            return result;
        }
        result.instructions = 1;
        z80Mclk_ += static_cast<std::uint64_t>(t) * kZ80Divider;
        return result;
    }
    stepCpu(result);
    return result;
}

std::uint64_t Machine::stateHash() const {
    state::StateHasher h;
    const m68k::State& c = cpu_->state();
    h.tag("M68K");
    for (const std::uint32_t r : c.d) {
        h.u32(r);
    }
    for (const std::uint32_t r : c.a) {
        h.u32(r);
    }
    h.u32(c.usp);
    h.u32(c.ssp);
    h.u32(c.pc);
    h.u16(c.sr);
    h.u16(c.ir);
    h.u16(c.irc);
    h.boolean(c.stopped);
    h.boolean(c.halted);
    h.i32(cpu_->irqLevel());

    const bus::BusState& b = bus_->state();
    h.tag("BUS ");
    h.bytes(b.ram);
    h.u32(b.tmssRegister);

    const vdp::VdpState& v = vdp_.state();
    h.tag("VDP ");
    h.bytes(v.regs);
    h.bytes(v.vram);
    h.bytes(v.satCache);
    for (const std::uint16_t w : v.cram) {
        h.u16(w);
    }
    for (const std::uint16_t w : v.vsram) {
        h.u16(w);
    }
    h.u16(v.address);
    h.u8(v.code);
    h.boolean(v.pending);
    h.boolean(v.fillPending);
    h.boolean(v.vintPending);
    h.boolean(v.hintPending);
    h.boolean(v.spriteOverflow);
    h.boolean(v.spriteCollision);
    h.boolean(v.oddFrame);
    h.boolean(v.pal);
    h.i32(v.hintCounter);
    h.i32(v.line);
    h.u16(v.addressHigh);
    h.boolean(v.z80Int);
    h.boolean(v.vintArmed);
    h.u16(v.readBuffer);
    h.i32(v.fifoUsed);
    h.boolean(v.dmaActive);
    h.u32(v.dmaRemaining);
    h.u32(v.dmaSource);
    h.u64(v.dmaCredit);
    h.u32(v.dmaBusyBytes);
    h.boolean(v.prevLineDotOverflow);

    const bus::IoState& io = io_.state();
    h.tag("IO  ");
    h.u8(io.version);
    h.bytes(io.data);
    h.bytes(io.control);
    h.bytes(io.serial);
    h.u16(io.buttons[0]);
    h.u16(io.buttons[1]);
    for (std::size_t p = 0; p < 2; ++p) {
        h.u8(static_cast<std::uint8_t>(io.padType[p]));
        h.boolean(io.th[p]);
        h.u8(io.thCount[p]);
        h.u64(io.thChangedAt[p]);
    }

    const z80::Z80ControlState& z = z80_.state();
    h.tag("Z80 ");
    h.bytes(z.ram);
    h.boolean(z.busRequested);
    h.boolean(z.resetAsserted);
    h.u16(z.bank);
    h.u8(z.ymAddress[0]);
    h.u8(z.ymAddress[1]);
    h.bytes(z.ymRegs[0]);
    h.bytes(z.ymRegs[1]);
    h.u8(z.psgLast);
    h.u8(z.psgLatch);
    const z80::Z80State& zc = z80cpu_->state();
    h.tag("Z80C");
    for (const std::uint8_t r : {zc.a, zc.f, zc.b, zc.c, zc.d, zc.e, zc.h, zc.l, zc.i, zc.r, zc.im, zc.q}) {
        h.u8(r);
    }
    for (const std::uint16_t r : {zc.af2, zc.bc2, zc.de2, zc.hl2, zc.ix, zc.iy, zc.sp, zc.pc, zc.wz}) {
        h.u16(r);
    }
    for (const bool flag : {zc.iff1, zc.iff2, zc.halted, zc.eiDelay, zc.ldAir, zc.irqLine, zc.nmiPending}) {
        h.boolean(flag);
    }
    h.u64(z80Mclk_);

    for (const auto& d : devices_) {
        d->hashState(h);
    }

    const_cast<audio::SoundSystem&>(sound_).hashState(h);  // serializing ymfm is not const

    h.tag("SCHD");
    h.u64(frame_);
    h.i32(line_);
    h.u64(clock_.lineStart);
    h.u64(clock_.cpuMclk);
    h.u64(vintAt_);
    return h.digest();
}

}  // namespace gen
