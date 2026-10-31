#include "core/z80/z80_control.hpp"

#include "core/m68k/m68k.hpp"

namespace gen::z80 {

void Z80Control::reset() {
    s_ = Z80ControlState{};
    m68kStall_ = 0;
}

void Z80Control::writeReset(bool bit8) {
    const bool held = !bit8;
    if (held) {
        // /RESET resets the Z80 CPU and the YM2612 (spec 06); the register stubs forget the address latch.
        if (cpu_ != nullptr) {
            cpu_->reset();
        }
        s_.ymAddress[0] = 0;
        s_.ymAddress[1] = 0;
        if (sound_ != nullptr && !s_.resetAsserted) {
            sound_->ymReset();
        }
    }
    s_.resetAsserted = held;
}

// ---- Sound ports
// -------------------------------------------------------------------------------------------

std::uint8_t Z80Control::ymRead() {
    return sound_ != nullptr ? sound_->ymStatus() : 0x00;  // stub: not busy, no timer overflow
}

void Z80Control::ymWrite(int port, std::uint8_t value, CpuId cpu, std::uint32_t pc) {
    const int part = (port >> 1) & 1;
    if ((port & 1) == 0) {
        s_.ymAddress[part] = value;
    } else {
        const std::uint8_t reg = s_.ymAddress[part];
        s_.ymRegs[static_cast<std::size_t>(part)][reg] = value;
        log_.push(cpu, audio::Chip::Ym2612, static_cast<std::uint8_t>(part), reg, value, pc);
    }
    if (sound_ != nullptr) {
        sound_->ymWrite(port & 3, value);
    }
}

void Z80Control::psgWrite(std::uint8_t value, CpuId cpu, std::uint32_t pc) {
    if ((value & 0x80) != 0) {
        s_.psgLatch = static_cast<std::uint8_t>((value >> 4) & 7);
    }
    s_.psgLast = value;
    log_.push(cpu, audio::Chip::Psg, 0, s_.psgLatch, value, pc);
    if (sound_ != nullptr) {
        sound_->psgWrite(value);
    }
}

// ---- 68k side
// ----------------------------------------------------------------------------------------------

bool Z80Control::read68k(std::uint16_t addr, std::uint8_t& out) {
    if (addr < 0x4000) {
        out = s_.ram[addr & (kRamSize - 1)];
        return true;
    }
    if (addr < 0x6000) {
        out = ymRead();
        return true;
    }
    return false;  // bank register / PSG / VDP / banked window: not readable from the 68k side
}

void Z80Control::write68k(std::uint16_t addr, std::uint8_t value, std::uint32_t pc) {
    if (addr < 0x4000) {
        s_.ram[addr & (kRamSize - 1)] = value;
    } else if (addr < 0x6000) {
        ymWrite(addr & 3, value, CpuId::M68k, pc);
    } else if (addr < 0x6100) {
        // Serial bank register: each write shifts bit 0 in as the new address bit 23.
        s_.bank = static_cast<std::uint16_t>(((s_.bank >> 1) | ((value & 1) << 8)) & 0x1FF);
    } else if (addr >= 0x7F10 && addr < 0x7F18 && (addr & 1) != 0) {
        psgWrite(value, CpuId::M68k, pc);
    }
}

// ---- Z80 side
// ----------------------------------------------------------------------------------------------

void Z80Control::reportWindow(AccessKind kind, std::uint16_t addr, std::uint32_t m68kAddr,
                              std::uint8_t value) {
    waitCycles += timing_.z80Wait;
    m68kStall_ += timing_.m68kStall;
    if (hooks_ != nullptr && hooks_->onZ80Window != nullptr) {
        hooks_->onZ80Window(hooks_->ctx, kind, addr, m68kAddr, value,
                            cpu_ != nullptr ? cpu_->currentPc() : 0);
    }
}

std::uint8_t Z80Control::read8(std::uint16_t addr) {
    if (addr < 0x4000) {
        return s_.ram[addr & (kRamSize - 1)];
    }
    if (addr < 0x6000) {
        return ymRead();
    }
    if (addr >= 0x8000) {
        const std::uint32_t m = windowAddress(addr);
        const std::uint8_t v = bus68k_ != nullptr ? bus68k_->read8(m) : 0xFF;
        reportWindow(AccessKind::Read, addr, m, v);
        return v;
    }
    if (addr >= 0x7F00 && addr < 0x7F20 && bus68k_ != nullptr) {
        return bus68k_->read8(0xC00000u | (addr & 0x1Fu));  // VDP (spec 04: as for the 68k)
    }
    return 0xFF;  // bank register, unused: the bus floats high
}

void Z80Control::write8(std::uint16_t addr, std::uint8_t value) {
    const std::uint32_t pc = cpu_ != nullptr ? cpu_->currentPc() : 0;
    if (addr < 0x4000) {
        s_.ram[addr & (kRamSize - 1)] = value;
    } else if (addr < 0x6000) {
        ymWrite(addr & 3, value, CpuId::Z80, pc);
    } else if (addr < 0x6100) {
        s_.bank = static_cast<std::uint16_t>(((s_.bank >> 1) | ((value & 1) << 8)) & 0x1FF);
    } else if (addr >= 0x8000) {
        const std::uint32_t m = windowAddress(addr);
        reportWindow(AccessKind::Write, addr, m, value);
        if (bus68k_ != nullptr) {
            bus68k_->write8(m, value);
        }
    } else if (addr >= 0x7F00 && addr < 0x7F20) {
        if (addr >= 0x7F10 && addr < 0x7F18) {
            if ((addr & 1) != 0) {
                psgWrite(value, CpuId::Z80, pc);
            }
        } else if (bus68k_ != nullptr) {
            bus68k_->write8(0xC00000u | (addr & 0x1Fu), value);
        }
    }
}

}  // namespace gen::z80
