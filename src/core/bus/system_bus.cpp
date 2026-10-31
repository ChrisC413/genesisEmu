#include "core/bus/system_bus.hpp"

#include <algorithm>

namespace gen::bus {

const char* toString(BusIssue issue) {
    switch (issue) {
        case BusIssue::Unmapped:
            return "unmapped";
        case BusIssue::RomWrite:
            return "rom-write";
        case BusIssue::Z80NotOwned:
            return "z80-not-owned";
        case BusIssue::VdpLocked:
            return "vdp-locked";
    }
    return "?";
}

SystemBus::SystemBus(vdp::Vdp& vdp, Io& io, z80::Z80Control& z80, const BusClock& clock)
    : vdp_(vdp), io_(io), z80_(z80), clock_(clock) {
    vdp_.setDmaReader(&SystemBus::dmaRead, this);
}

void SystemBus::attach(const std::vector<std::uint8_t>& rom, m68k::M68k& cpu, bool tmss) {
    rom_ = &rom;
    cpu_ = &cpu;
    tmss_ = tmss;
    pages_.fill(PageKind::Unmapped);
    fastRead.fill(nullptr);
    fastWrite.fill(nullptr);
    // Cartridge: $000000-$3FFFFF, only the pages the image covers (the Machine pads it to 64 KB pages).
    const std::size_t romPages = std::min<std::size_t>(rom.size() >> 16, 0x40);
    for (std::size_t p = 0; p < romPages; ++p) {
        pages_[p] = PageKind::Rom;
        fastRead[p] = rom.data() + (p << 16);
    }
    pages_[0xA0] = PageKind::Z80;
    pages_[0xA1] = PageKind::Io;
    for (std::size_t p = 0xC0; p <= 0xDF; ++p) {
        pages_[p] = PageKind::Vdp;
    }
    // Work RAM: 64 KB mirrored over $E00000-$FFFFFF.
    for (std::size_t p = 0xE0; p <= 0xFF; ++p) {
        pages_[p] = PageKind::Ram;
        fastRead[p] = state_.ram.data();
        fastWrite[p] = state_.ram.data();
    }
}

void SystemBus::reset() {
    state_ = BusState{};
    waitCycles = 0;
}

void SystemBus::clearIssues() {
    issues_.clear();
    issueCounts_.fill(0);
}

std::uint16_t SystemBus::openBus() const {
    // Approximation (spec 04): the last prefetched word.
    return cpu_ != nullptr ? cpu_->state().irc : 0;
}

void SystemBus::report(BusIssue issue, std::uint32_t addr, std::uint8_t size, std::uint32_t value,
                       bool write) {
    ++issueCounts_[static_cast<std::size_t>(issue)];
    if (issues_.size() < kIssueLogLimit) {
        issues_.push_back(
            BusIssueRecord{addr, cpu_ != nullptr ? cpu_->currentPc() : 0, value, size, write, issue});
    }
}

int SystemBus::iack(int level) {
    vdp_.acknowledge(level);
    updateIrq();
    return kAutovector;
}

void SystemBus::updateIrq() {
    if (cpu_ != nullptr) {
        cpu_->setIrqLevel(vdp_.irqLevel());
    }
}

std::uint16_t SystemBus::dmaRead(void* ctx, std::uint32_t addr) {
    auto& self = *static_cast<SystemBus*>(ctx);
    addr &= 0xFFFFFE;
    if (const std::uint8_t* page = self.fastRead[addr >> 16]) {
        return static_cast<std::uint16_t>((page[addr & 0xFFFF] << 8) | page[(addr & 0xFFFF) + 1]);
    }
    return self.openBus();
}

// ---- Dispatch
// ----------------------------------------------------------------------------------------------

std::uint8_t SystemBus::read8(std::uint32_t addr) {
    addr &= 0xFFFFFF;
    const std::uint16_t odd = addr & 1;
    switch (pages_[addr >> 16]) {
        case PageKind::Rom:
        case PageKind::Ram:
            return fastRead[addr >> 16][addr & 0xFFFF];
        case PageKind::Z80:
            return static_cast<std::uint8_t>(readZ80(addr, 1));
        case PageKind::Io:
            return static_cast<std::uint8_t>(readIo(addr, 1));
        case PageKind::Vdp: {
            const std::uint16_t w = readVdp(addr & ~1u, 1);
            return static_cast<std::uint8_t>(odd != 0 ? w : w >> 8);
        }
        case PageKind::Unmapped:
            break;
    }
    const std::uint16_t ob = openBus();
    report(BusIssue::Unmapped, addr, 1, 0, false);
    return static_cast<std::uint8_t>(odd != 0 ? ob : ob >> 8);
}

std::uint16_t SystemBus::read16(std::uint32_t addr) {
    addr &= 0xFFFFFE;
    switch (pages_[addr >> 16]) {
        case PageKind::Rom:
        case PageKind::Ram: {
            const std::uint8_t* p = fastRead[addr >> 16] + (addr & 0xFFFF);
            return static_cast<std::uint16_t>((p[0] << 8) | p[1]);
        }
        case PageKind::Z80:
            return readZ80(addr, 2);
        case PageKind::Io:
            return readIo(addr, 2);
        case PageKind::Vdp:
            return readVdp(addr, 2);
        case PageKind::Unmapped:
            break;
    }
    report(BusIssue::Unmapped, addr, 2, 0, false);
    return openBus();
}

void SystemBus::write8(std::uint32_t addr, std::uint8_t value) {
    addr &= 0xFFFFFF;
    switch (pages_[addr >> 16]) {
        case PageKind::Ram:
            state_.ram[addr & 0xFFFF] = value;
            return;
        case PageKind::Rom:
            report(BusIssue::RomWrite, addr, 1, value, true);
            return;
        case PageKind::Z80:
            writeZ80(addr, value, 1);
            return;
        case PageKind::Io:
            writeIo(addr, value, 1);
            return;
        case PageKind::Vdp:
            writeVdp(addr, value, 1);
            return;
        case PageKind::Unmapped:
            break;
    }
    report(BusIssue::Unmapped, addr, 1, value, true);
}

void SystemBus::write16(std::uint32_t addr, std::uint16_t value) {
    addr &= 0xFFFFFE;
    switch (pages_[addr >> 16]) {
        case PageKind::Ram:
            state_.ram[addr & 0xFFFF] = static_cast<std::uint8_t>(value >> 8);
            state_.ram[(addr & 0xFFFF) + 1] = static_cast<std::uint8_t>(value);
            return;
        case PageKind::Rom:
            report(BusIssue::RomWrite, addr, 2, value, true);
            return;
        case PageKind::Z80:
            // Word writes reach the Z80 bus as their high byte.
            writeZ80(addr, static_cast<std::uint8_t>(value >> 8), 2);
            return;
        case PageKind::Io:
            writeIo(addr, value, 2);
            return;
        case PageKind::Vdp:
            writeVdp(addr, value, 2);
            return;
        case PageKind::Unmapped:
            break;
    }
    report(BusIssue::Unmapped, addr, 2, value, true);
}

// ---- Z80 area ($A00000-$A0FFFF)
// ----------------------------------------------------------------------------

std::uint16_t SystemBus::readZ80(std::uint32_t addr, std::uint8_t size) {
    const std::uint16_t ob = openBus();
    if (!z80_.busGranted()) {
        report(BusIssue::Z80NotOwned, addr, size, 0, false);
        return size == 2 ? ob : static_cast<std::uint8_t>((addr & 1) != 0 ? ob : ob >> 8);
    }
    std::uint8_t b = 0;
    if ((addr & 0xFFFF) >= 0x8000 || !z80_.read(static_cast<std::uint16_t>(addr & 0x7FFF), b)) {
        report(BusIssue::Unmapped, addr, size, 0, false);
        return size == 2 ? ob : static_cast<std::uint8_t>((addr & 1) != 0 ? ob : ob >> 8);
    }
    // A word read returns the byte in both halves.
    return size == 2 ? static_cast<std::uint16_t>((b << 8) | b) : b;
}

void SystemBus::writeZ80(std::uint32_t addr, std::uint8_t value, std::uint8_t size) {
    if (!z80_.busGranted()) {
        report(BusIssue::Z80NotOwned, addr, size, value, true);
        return;
    }
    if ((addr & 0xFFFF) >= 0x8000) {
        report(BusIssue::Unmapped, addr, size, value, true);
        return;
    }
    z80_.write(static_cast<std::uint16_t>(addr & 0x7FFF), value);
}

// ---- I/O and control ($A10000-$A1FFFF)
// ---------------------------------------------------------------------

std::uint16_t SystemBus::readIo(std::uint32_t addr, std::uint8_t size) {
    const std::uint32_t off = addr & 0xFFFF;
    const std::uint16_t ob = openBus();
    const auto byteOf = [&](std::uint16_t w) {
        return size == 2 ? w : static_cast<std::uint16_t>((addr & 1) != 0 ? (w & 0xFF) : (w >> 8));
    };
    if (off < 0x20) {
        // Byte registers at odd addresses; the even byte mirrors them.
        const std::uint8_t v = io_.read(off >> 1, clock_.cpuMclk);
        return size == 2 ? static_cast<std::uint16_t>((v << 8) | v) : v;
    }
    if (off >= 0x1100 && off < 0x1200) {
        // Bit 8 (bit 0 of the even byte) reads 0 while the bus is granted.
        const std::uint16_t busy = z80_.busGranted() ? 0 : 0x0100;
        return byteOf(static_cast<std::uint16_t>((ob & 0xFEFF) | busy));
    }
    if ((off >= 0x1000 && off < 0x1100) || (off >= 0x1200 && off < 0x1300) ||
        (off >= 0x4000 && off < 0x4004)) {
        return byteOf(ob);  // write-only registers
    }
    report(BusIssue::Unmapped, addr, size, 0, false);
    return byteOf(ob);
}

void SystemBus::writeIo(std::uint32_t addr, std::uint16_t value, std::uint8_t size) {
    const std::uint32_t off = addr & 0xFFFF;
    const bool odd = (addr & 1) != 0;
    // The control registers decode bit 8 of a word, i.e. bit 0 of a byte written to the even address.
    const bool bit8 = size == 2 ? (value & 0x0100) != 0 : (value & 0x01) != 0;
    if (off < 0x20) {
        io_.write(off >> 1, static_cast<std::uint8_t>(value), clock_.cpuMclk);
        return;
    }
    if (off >= 0x1000 && off < 0x1100) {
        return;  // $A11000 memory mode (DRAM refresh for development carts): no effect
    }
    if (off >= 0x1100 && off < 0x1200) {
        if (size == 2 || !odd) {
            z80_.writeBusRequest(bit8);
        }
        return;
    }
    if (off >= 0x1200 && off < 0x1300) {
        if (size == 2 || !odd) {
            z80_.writeReset(bit8);
        }
        return;
    }
    if (off == 0x30F1 && size == 1) {
        state_.sramControl = static_cast<std::uint8_t>(value);  // SRAM itself arrives in M04-08
        return;
    }
    if (off >= 0x4000 && off < 0x4004) {
        const unsigned shift = (3 - (off & 3)) * 8;
        std::uint32_t mask = size == 2 ? 0xFFFFu << (shift - 8) : 0xFFu << shift;
        std::uint32_t v = size == 2 ? static_cast<std::uint32_t>(value) << (shift - 8)
                                    : static_cast<std::uint32_t>(value & 0xFF) << shift;
        state_.tmssRegister = (state_.tmssRegister & ~mask) | (v & mask);
        return;
    }
    // Other /TIME ($A130xx) registers belong to the cartridge device chain (spec 04), not modeled yet.
    report(BusIssue::Unmapped, addr, size, value, true);
}

// ---- VDP ($C00000-$DFFFFF, mirrored every 32 bytes)
// --------------------------------------------------------

std::uint16_t SystemBus::readVdp(std::uint32_t addr, std::uint8_t size) {
    if (vdpLocked()) {
        report(BusIssue::VdpLocked, addr, size, 0, false);
        return openBus();
    }
    const std::uint32_t port = addr & 0x1F;
    if (port < 0x04) {
        return vdp_.readData();
    }
    if (port < 0x08) {
        return vdp_.readStatus(lineMclk());
    }
    if (port < 0x10) {
        return vdp_.readHvCounter(lineMclk());
    }
    report(BusIssue::Unmapped, addr, size, 0, false);
    return openBus();
}

void SystemBus::writeVdp(std::uint32_t addr, std::uint16_t value, std::uint8_t size) {
    if (vdpLocked()) {
        report(BusIssue::VdpLocked, addr, size, value, true);
        return;
    }
    const std::uint32_t port = addr & 0x1F;
    if (port < 0x08) {
        // B-5: a byte write to the data or control port puts the byte on both halves of the word.
        const std::uint16_t w =
            size == 2 ? value : static_cast<std::uint16_t>(((value & 0xFF) << 8) | (value & 0xFF));
        if (port < 0x04) {
            vdp_.writeData(w);
        } else {
            vdp_.writeControl(w);
            updateIrq();
        }
        return;
    }
    if (port >= 0x10 && port < 0x18) {
        if (size == 2 || (addr & 1) != 0) {
            z80_.state().psgLast = static_cast<std::uint8_t>(value);  // PSG stub until M05-02
        }
        return;
    }
    if (port < 0x10) {
        return;  // HV counter is read-only
    }
    report(BusIssue::Unmapped, addr, size, value, true);
}

}  // namespace gen::bus
