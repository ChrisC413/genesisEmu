#pragma once
// YM2612 FM synthesis behind IFmSynth (spec 06, D-013). The interface keeps ymfm out of the bus and save-state
// code; YmfmSynth is the implementation (third_party/ymfm, BSD-3-Clause).

#include <cstdint>
#include <memory>
#include <vector>

namespace gen::audio {

inline constexpr int kFmClockDivider = 7;     // YM2612 input clock = MCLK / 7
inline constexpr int kFmSampleClocks = 144;   // one output sample per 144 input clocks (~53,267 Hz NTSC)
inline constexpr int kFmChannelDac = 6;       // channel-mask bit of the DAC (channels 0-5 are FM 1-6)

class IFmSynth {
public:
    virtual ~IFmSynth() = default;
    virtual void reset() = 0;
    // `port` 0-3 = A0 (address, part I), D0 (data), A1 (address, part II), D1.
    virtual void write(int port, std::uint8_t value) = 0;
    // Status: bit 7 busy, bit 1 timer B overflow, bit 0 timer A overflow.
    virtual std::uint8_t readStatus() = 0;
    // Renders `frames` stereo samples (interleaved L/R) at the native rate. Channels whose bit is clear in
    // `channelMask` (bits 0-5 = FM 1-6, bit 6 = DAC) are silent; timers and envelopes still advance.
    virtual void generate(std::int32_t* stereo, int frames, unsigned channelMask) = 0;
    virtual std::vector<std::uint8_t> serialize() = 0;
    virtual void deserialize(const std::vector<std::uint8_t>& data) = 0;
};

std::unique_ptr<IFmSynth> makeYmfmSynth();

}  // namespace gen::audio
