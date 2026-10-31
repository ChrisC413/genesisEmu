#pragma once
// Texas Instruments SN76489 as integrated into the Genesis VDP (spec 06 "SN76489", M05-02): three square tone
// channels and one noise channel with a 16-bit LFSR (Sega taps: bits 0 and 3), 4-bit attenuation in 2 dB
// steps. Clocked at MCLK/15; the counters advance once every 16 clocks (one "tick").

#include <array>
#include <cstdint>

namespace gen::audio {

inline constexpr int kPsgMaxLevel = 2000;  // output amplitude of one channel at attenuation 0

struct PsgState {
    std::array<std::uint16_t, 3> period{};  // 10-bit tone periods
    std::array<std::uint8_t, 4> attenuation{15, 15, 15, 15};
    std::uint8_t noiseControl = 0;  // bit 2 = white noise, bits 0-1 = rate
    std::uint8_t latch = 0;         // register selected by the last latch byte (0-7)
    std::array<std::uint16_t, 4> counter{};
    std::array<std::int8_t, 4> output{1, 1, 1, 1};  // +1 / -1 square output of each channel
    std::uint16_t lfsr = 0x8000;
    bool noiseFlip = false;  // the noise counter's flip-flop; the LFSR shifts on each rising edge
};

class Psg {
public:
    void reset() { s_ = PsgState{}; }
    void write(std::uint8_t value);

    // Advances one tick (16 PSG clocks).
    void tick();
    // Instantaneous level of channel `ch` (0-2 tone, 3 noise), signed.
    int level(int ch) const;
    // Sum of the channels whose bit is set in `mask` (bit 0-2 tone, bit 3 noise).
    int mix(unsigned mask = 0xF) const;

    static int volume(int attenuation);  // amplitude for an attenuation value (15 = silent)

    PsgState& state() { return s_; }
    const PsgState& state() const { return s_; }

private:
    PsgState s_;
};

}  // namespace gen::audio
