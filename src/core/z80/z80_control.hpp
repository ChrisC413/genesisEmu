#pragma once
// The Z80 side of the system as the 68k sees it (spec 04 "Z80 memory map"): 8 KB sound RAM, the BUSREQ /
// RESET control registers ($A11100 / $A11200), the bank register and YM2612 / PSG write stubs. The Z80 CPU
// itself does not execute yet (M04-04); games can still load their sound drivers into Z80 RAM.

#include <array>
#include <cstdint>

namespace gen::z80 {

inline constexpr std::size_t kRamSize = 0x2000;

struct Z80ControlState {
    std::array<std::uint8_t, kRamSize> ram{};
    bool busRequested = false;  // 68k asked for the Z80 bus ($A11100 bit 8 = 1)
    bool resetAsserted = true;  // Z80 /RESET held ($A11200 bit 8 = 0); asserted at power on
    std::uint16_t bank = 0;     // 9-bit bank register: 68k address bits 15-23 of the $8000 window
    std::uint8_t ymAddress[2]{};
    std::array<std::array<std::uint8_t, 256>, 2> ymRegs{};  // last value written per YM2612 register
    std::uint8_t psgLast = 0;
};

class Z80Control {
public:
    void reset();

    // The 68k owns the Z80 bus: Z80-space accesses from the 68k are valid. The grant is immediate.
    // Divergence from some references: the grant does not depend on the reset line.
    bool busGranted() const { return s_.busRequested; }

    // $A11100 / $A11200 control-register writes; `bit8` is bit 8 of a word write or bit 0 of a byte write
    // to the even address.
    void writeBusRequest(bool bit8) { s_.busRequested = bit8; }
    void writeReset(bool bit8);

    // Z80-space accesses ($0000-$7FFF of the Z80 map; `addr` is the Z80 address). Returns false from
    // read() when the address has no readable device for the 68k (caller substitutes open bus).
    bool read(std::uint16_t addr, std::uint8_t& out) const;
    void write(std::uint16_t addr, std::uint8_t value);

    Z80ControlState& state() { return s_; }
    const Z80ControlState& state() const { return s_; }

private:
    Z80ControlState s_;
};

}  // namespace gen::z80
