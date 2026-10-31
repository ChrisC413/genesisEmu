#pragma once
// I/O chip at $A10000-$A1001F (spec 04 "I/O registers"): version register, controller data/control
// registers and stubbed serial registers. Controller ports 1 and 2 speak the TH-line protocol of the 3- and
// 6-button pads (M04-01); port 3 (EXT) has nothing connected.

#include <array>
#include <cstdint>

namespace gen::bus {

// Console region: decides the version register and the video timing (Europe = PAL).
enum class Region : std::uint8_t { Japan, Americas, Europe };

// Picks the console region from the cartridge header's region mask (cart::RegionMask). NTSC first (D-020):
// Americas, then Japan, then Europe; Americas when the header declares nothing.
Region regionFromHeader(std::uint8_t headerRegions);
const char* toString(Region region);

// Pad buttons, active high in this mask (the port reads them active low).
enum PadButton : std::uint16_t {
    kPadUp = 1 << 0,
    kPadDown = 1 << 1,
    kPadLeft = 1 << 2,
    kPadRight = 1 << 3,
    kPadB = 1 << 4,
    kPadC = 1 << 5,
    kPadA = 1 << 6,
    kPadStart = 1 << 7,
    kPadX = 1 << 8,  // 6-button pad only
    kPadY = 1 << 9,
    kPadZ = 1 << 10,
    kPadMode = 1 << 11,
};

enum class PadType : std::uint8_t { None, ThreeButton, SixButton };

// The 6-button pad's TH counter resets when TH has not changed for ~1.5 ms (in master clocks).
inline constexpr std::uint64_t kSixButtonTimeoutMclk = 80'000;

struct IoState {
    std::uint8_t version = 0;
    std::array<std::uint8_t, 3> data{};     // $A10003/5/7: last written output bits
    std::array<std::uint8_t, 3> control{};  // $A10009/B/D: 1 = pin is an output
    std::array<std::uint8_t, 9> serial{};   // $A1000F-$A1001F (odd addresses): stored, no transfer
    std::array<std::uint16_t, 2>
        buttons{};  // pressed buttons of pads 1 and 2 (PadButton mask), latched per frame
    std::array<PadType, 2> padType{PadType::ThreeButton, PadType::ThreeButton};
    std::array<bool, 2> th{true, true};          // TH level the pad sees
    std::array<std::uint8_t, 2> thCount{};       // 6-button: TH falling edges since the last timeout
    std::array<std::uint64_t, 2> thChangedAt{};  // master clock of the last TH change
};

class Io {
public:
    void reset(Region region, bool pal, bool tmss);

    // `reg` is the register index (address - $A10000) / 2, 0-15. `mclk` is the current master clock (for the
    // 6-button timeout).
    std::uint8_t read(std::uint32_t reg, std::uint64_t mclk = 0);
    void write(std::uint32_t reg, std::uint8_t value, std::uint64_t mclk = 0);

    void setPadType(int port, PadType type) { s_.padType[static_cast<std::size_t>(port & 1)] = type; }

    IoState& state() { return s_; }
    const IoState& state() const { return s_; }

private:
    std::uint8_t readData(int port, std::uint64_t mclk);
    void updateTh(int port, std::uint64_t mclk);
    IoState s_;
};

}  // namespace gen::bus
