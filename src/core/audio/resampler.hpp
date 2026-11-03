#pragma once
// Band-limited stereo resampler (spec 06 "Mixing and output", M05-01): windowed-sinc interpolation with a
// polyphase table, cutoff just below the lower Nyquist rate. The ratio can be nudged at run time for dynamic
// rate control (the frontend keeps the host audio queue near a target fill).

#include <cstdint>
#include <span>
#include <vector>

namespace gen::audio {

class Resampler {
public:
    // Kernel half-width in input samples and phase resolution.
    static constexpr int kHalfTaps = 16;
    static constexpr int kPhases = 256;

    Resampler() = default;
    Resampler(double inRate, double outRate) { setRates(inRate, outRate); }

    void setRates(double inRate, double outRate);
    // Multiplies the input step: > 1 consumes input faster (fewer output samples). Clamped to +-0.5%.
    void setRatioAdjust(double factor);
    double ratioAdjust() const { return adjust_; }
    void reset();

    // Consumes interleaved stereo input and appends interleaved stereo output.
    void process(std::span<const std::int16_t> in, std::vector<std::int16_t>& out);

private:
    double inRate_ = 1;
    double outRate_ = 1;
    double adjust_ = 1;
    double step_ = 1;           // input samples per output sample
    double pos_ = 0;            // position of the next output sample, relative to hist_[0]
    std::vector<float> table_;  // (kPhases + 1) x (2 * kHalfTaps)
    std::vector<float> hist_;   // interleaved stereo input not yet fully consumed
};

}  // namespace gen::audio
