#pragma once
// I/O chip at $A10000-$A1001F (spec 04 "I/O registers"): version register, controller data/control
// registers and stubbed serial registers. Controller input arrives in M04-01; until then every pad is idle.

#include <array>
#include <cstdint>

namespace gen::bus {

// Console region: decides the version register and the video timing (Europe = PAL).
enum class Region : std::uint8_t { Japan, Americas, Europe };

// Picks the console region from the cartridge header's region mask (cart::RegionMask). NTSC first (D-020):
// Americas, then Japan, then Europe; Americas when the header declares nothing.
Region regionFromHeader(std::uint8_t headerRegions);
const char* toString(Region region);

// 3-button pad buttons, active high in this mask (the port reads them active low).
enum PadButton : std::uint16_t {
    kPadUp = 1 << 0,
    kPadDown = 1 << 1,
    kPadLeft = 1 << 2,
    kPadRight = 1 << 3,
    kPadB = 1 << 4,
    kPadC = 1 << 5,
    kPadA = 1 << 6,
    kPadStart = 1 << 7,
};

struct IoState {
    std::uint8_t version = 0;
    std::array<std::uint8_t, 3> data{};     // $A10003/5/7: last written output bits
    std::array<std::uint8_t, 3> control{};  // $A10009/B/D: 1 = pin is an output
    std::array<std::uint8_t, 9> serial{};   // $A1000F-$A1001F (odd addresses): stored, no transfer
    std::array<std::uint16_t, 2> buttons{};  // pressed buttons of pads 1 and 2 (PadButton mask)
};

class Io {
public:
    void reset(Region region, bool pal, bool tmss);

    // `reg` is the register index (address - $A10000) / 2, 0-15.
    std::uint8_t read(std::uint32_t reg) const;
    void write(std::uint32_t reg, std::uint8_t value);

    IoState& state() { return s_; }
    const IoState& state() const { return s_; }

private:
    std::uint8_t readData(int port) const;
    IoState s_;
};

}  // namespace gen::bus
