#include "core/z80/z80_control.hpp"

namespace gen::z80 {

void Z80Control::reset() {
    s_ = Z80ControlState{};
}

void Z80Control::writeReset(bool bit8) {
    const bool held = !bit8;
    if (held && !s_.resetAsserted) {
        // Z80 reset also resets the YM2612 (spec 06); the register stubs forget their state.
        s_.ymAddress[0] = 0;
        s_.ymAddress[1] = 0;
    }
    s_.resetAsserted = held;
}

bool Z80Control::read(std::uint16_t addr, std::uint8_t& out) const {
    if (addr < 0x4000) {
        out = s_.ram[addr & (kRamSize - 1)];
        return true;
    }
    if (addr < 0x6000) {
        out = 0x00;  // YM2612 status: not busy, no timer overflow (stub until M05)
        return true;
    }
    return false;  // bank register / PSG / VDP / banked window: not readable from the 68k side
}

void Z80Control::write(std::uint16_t addr, std::uint8_t value) {
    if (addr < 0x4000) {
        s_.ram[addr & (kRamSize - 1)] = value;
    } else if (addr < 0x6000) {
        const int part = (addr >> 1) & 1;
        if ((addr & 1) == 0) {
            s_.ymAddress[part] = value;
        } else {
            s_.ymRegs[static_cast<std::size_t>(part)][s_.ymAddress[part]] = value;
        }
    } else if (addr < 0x6100) {
        // Serial bank register: each write shifts bit 0 in as the new address bit 23.
        s_.bank = static_cast<std::uint16_t>(((s_.bank >> 1) | ((value & 1) << 8)) & 0x1FF);
    } else if (addr == 0x7F11 || addr == 0x7F13 || addr == 0x7F15 || addr == 0x7F17) {
        s_.psgLast = value;
    }
}

}  // namespace gen::z80
