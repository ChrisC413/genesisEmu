#pragma once
// The Z80 side of the system (spec 04 "Z80 memory map", M04-07): 8 KB sound RAM, the BUSREQ / RESET control
// registers ($A11100 / $A11200) as the 68k sees them, the bank register and 68k window, and the sound-chip
// ports. It is also the Z80 CPU's bus (BusZ80).
//
// BUSREQ / RESET (B-3, CZ-7): the Z80 runs only while neither is asserted. The 68k owns the Z80 bus (may
// access $A00000-$A0FFFF, and $A11100 bit 8 reads 0) only while BUSREQ is asserted *and* RESET is released.
// Asserting RESET resets the Z80 CPU (and the YM2612).

#include <array>
#include <cstdint>

#include "core/audio/register_log.hpp"
#include "core/hooks.hpp"
#include "core/z80/z80.hpp"

namespace gen::m68k {
class Bus68k;
}

namespace gen::z80 {

inline constexpr std::size_t kRamSize = 0x2000;

// Sound chips behind the Z80 / 68k ports (M05 attaches the YM2612 and PSG; null = register stubs only).
class SoundPorts {
public:
    virtual ~SoundPorts() = default;
    virtual void ymWrite(int port, std::uint8_t value) = 0;  // port 0-3 = A0 D0 A1 D1
    virtual std::uint8_t ymStatus() = 0;
    virtual void psgWrite(std::uint8_t value) = 0;
    virtual void ymReset() = 0;
};

struct Z80ControlState {
    std::array<std::uint8_t, kRamSize> ram{};
    bool busRequested = false;  // 68k asked for the Z80 bus ($A11100 bit 8 = 1)
    bool resetAsserted = true;  // Z80 /RESET held ($A11200 bit 8 = 0); asserted at power on
    std::uint16_t bank = 0;     // 9-bit bank register: 68k address bits 15-23 of the $8000 window
    std::uint8_t ymAddress[2]{};
    std::array<std::array<std::uint8_t, 256>, 2> ymRegs{};  // last value written per YM2612 register
    std::uint8_t psgLast = 0;
    std::uint8_t psgLatch = 0;  // PSG register selected by the last latch byte (channel * 2 + volume)
};

// Approximate cost of one Z80 access to the 68k window (spec 03): Z80 wait states and 68k stall cycles.
struct WindowTiming {
    int z80Wait = 3;
    int m68kStall = 11;
};

class Z80Control final : public BusZ80 {
public:
    void reset();

    // The CPU this control block drives, the 68k bus behind the bank window, and the hooks.
    void attach(Z80* cpu, m68k::Bus68k* bus68k, const Hooks* hooks) {
        cpu_ = cpu;
        bus68k_ = bus68k;
        hooks_ = hooks;
    }
    void setSound(SoundPorts* sound) { sound_ = sound; }
    void setWindowTiming(WindowTiming t) { timing_ = t; }

    // The 68k owns the Z80 bus: Z80-space accesses from the 68k are valid and $A11100 bit 8 reads 0.
    bool busGranted() const { return s_.busRequested && !s_.resetAsserted; }
    // The Z80 executes (neither BUSREQ nor RESET asserted).
    bool running() const { return !s_.busRequested && !s_.resetAsserted; }

    // $A11100 / $A11200 control-register writes; `bit8` is bit 8 of a word write or bit 0 of a byte write
    // to the even address.
    void writeBusRequest(bool bit8) { s_.busRequested = bit8; }
    void writeReset(bool bit8);

    // 68k accesses to Z80 space ($0000-$7FFF of the Z80 map; `addr` is the Z80 address). read68k() returns
    // false when the address has no readable device for the 68k (caller substitutes open bus).
    bool read68k(std::uint16_t addr, std::uint8_t& out);
    void write68k(std::uint16_t addr, std::uint8_t value, std::uint32_t pc);
    // A PSG write from the 68k ($C00011).
    void psgWrite(std::uint8_t value, CpuId cpu, std::uint32_t pc);

    // BusZ80: the Z80's own view.
    std::uint8_t read8(std::uint16_t addr) override;
    // Z80 RAM only; the YM, the bank window and the VDP read as $FF (reading them has side effects).
    std::uint8_t peek8(std::uint16_t addr) override {
        return addr < 0x4000 ? s_.ram[addr & (kRamSize - 1)] : 0xFF;
    }
    void write8(std::uint16_t addr, std::uint8_t value) override;

    // 68k cycles the 68k loses to Z80 window accesses since the last call.
    int takeM68kStall() {
        const int s = m68kStall_;
        m68kStall_ = 0;
        return s;
    }
    // Save states: the stall not yet charged to the 68k (non-zero only after a lone Z80 step).
    int pendingM68kStall() const { return m68kStall_; }
    void setPendingM68kStall(int cycles) { m68kStall_ = cycles; }

    audio::RegisterLog& log() { return log_; }
    const audio::RegisterLog& log() const { return log_; }

    Z80ControlState& state() { return s_; }
    const Z80ControlState& state() const { return s_; }

private:
    std::uint8_t ymRead();
    void ymWrite(int port, std::uint8_t value, CpuId cpu, std::uint32_t pc);
    std::uint32_t windowAddress(std::uint16_t addr) const {
        return (static_cast<std::uint32_t>(s_.bank) << 15) | (addr & 0x7FFFu);
    }
    void reportWindow(AccessKind kind, std::uint16_t addr, std::uint32_t m68kAddr, std::uint8_t value);

    Z80ControlState s_;
    Z80* cpu_ = nullptr;
    m68k::Bus68k* bus68k_ = nullptr;
    const Hooks* hooks_ = nullptr;
    SoundPorts* sound_ = nullptr;
    WindowTiming timing_;
    int m68kStall_ = 0;
    audio::RegisterLog log_;
};

}  // namespace gen::z80
