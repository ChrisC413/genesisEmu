#include "core/audio/resampler.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace gen::audio {

void Resampler::setRates(double inRate, double outRate) {
    inRate_ = inRate;
    outRate_ = outRate;
    // Cutoff at 90% of the lower Nyquist frequency, as a fraction of the input sample rate.
    const double fc = 0.45 * std::min(1.0, outRate / inRate);
    constexpr int taps = 2 * kHalfTaps;
    table_.assign(static_cast<std::size_t>((kPhases + 1) * taps), 0.0f);
    for (int p = 0; p <= kPhases; ++p) {
        const double frac = static_cast<double>(p) / kPhases;
        double sum = 0;
        for (int k = 0; k < taps; ++k) {
            const double t = (k - kHalfTaps + 1) - frac;  // distance from the output point, in input samples
            const double x = 2.0 * fc * t;
            const double sinc =
                std::abs(x) < 1e-9 ? 1.0 : std::sin(std::numbers::pi * x) / (std::numbers::pi * x);
            // Blackman window over [-kHalfTaps, kHalfTaps].
            const double w = (t + kHalfTaps) / (2.0 * kHalfTaps);
            const double win =
                0.42 - 0.5 * std::cos(2 * std::numbers::pi * w) + 0.08 * std::cos(4 * std::numbers::pi * w);
            const double h = 2.0 * fc * sinc * std::max(0.0, win);
            table_[static_cast<std::size_t>(p * taps + k)] = static_cast<float>(h);
            sum += h;
        }
        for (int k = 0; k < taps; ++k) {  // unity DC gain at every phase
            table_[static_cast<std::size_t>(p * taps + k)] /= static_cast<float>(sum);
        }
    }
    setRatioAdjust(adjust_);
    reset();
}

void Resampler::setRatioAdjust(double factor) {
    adjust_ = std::clamp(factor, 0.995, 1.005);
    step_ = inRate_ / outRate_ * adjust_;
}

void Resampler::reset() {
    // Start with a zero history so the first output sample has a full kernel behind it.
    hist_.assign(static_cast<std::size_t>(2 * (kHalfTaps - 1)), 0.0f);
    pos_ = kHalfTaps - 1;
}

void Resampler::process(std::span<const std::int16_t> in, std::vector<std::int16_t>& out) {
    for (const std::int16_t s : in) {
        hist_.push_back(static_cast<float>(s));
    }
    const auto frames = static_cast<double>(hist_.size() / 2);
    constexpr int taps = 2 * kHalfTaps;
    while (pos_ + kHalfTaps < frames) {
        const auto base = static_cast<int>(pos_);
        const double frac = pos_ - base;
        const double ph = frac * kPhases;
        const auto p0 = static_cast<int>(ph);
        const auto mix = static_cast<float>(ph - p0);
        const float* h0 = &table_[static_cast<std::size_t>(p0 * taps)];
        const float* h1 = h0 + taps;
        float l = 0;
        float r = 0;
        const float* x = &hist_[static_cast<std::size_t>(2 * (base - kHalfTaps + 1))];
        for (int k = 0; k < taps; ++k) {
            const float h = h0[k] + (h1[k] - h0[k]) * mix;
            l += h * x[2 * k];
            r += h * x[2 * k + 1];
        }
        out.push_back(static_cast<std::int16_t>(std::clamp(std::lround(l), -32768L, 32767L)));
        out.push_back(static_cast<std::int16_t>(std::clamp(std::lround(r), -32768L, 32767L)));
        pos_ += step_;
    }
    // Drop input no future output sample can reach.
    const int keepFrom = std::max(0, static_cast<int>(pos_) - kHalfTaps + 1);
    hist_.erase(hist_.begin(), hist_.begin() + 2 * keepFrom);
    pos_ -= keepFrom;
}

}  // namespace gen::audio
