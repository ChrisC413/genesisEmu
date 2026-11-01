#include "core/audio/sound.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

#include "core/state/archive.hpp"

namespace gen::audio {

namespace {

constexpr double kLowPassHz = 3390.0;
constexpr double kDcBlockHz = 10.0;

std::int16_t clip(float v) {
    return static_cast<std::int16_t>(std::clamp(std::lround(v), -32768L, 32767L));
}

}  // namespace

const char* channelName(int bit) {
    static constexpr const char* kNames[kChannelCount] = {"FM 1", "FM 2",  "FM 3",  "FM 4",  "FM 5", "FM 6",
                                                          "DAC",  "PSG 1", "PSG 2", "PSG 3", "Noise"};
    return bit >= 0 && bit < kChannelCount ? kNames[bit] : "?";
}

SoundSystem::SoundSystem() : fm_(makeYmfmSynth()), hasher_(std::make_unique<state::StateHasher>()) {}

SoundSystem::~SoundSystem() = default;

void SoundSystem::reset(bool pal) {
    pal_ = pal;
    fm_ = makeYmfmSynth();  // a fresh chip: ymfm's reset() keeps some internal counters
    psg_.reset();
    mclk_ = 0;
    psgNext_ = 0;
    lpL_ = 0;
    lpR_ = 0;
    dcIn_ = {};
    dcOut_ = {};
    out_.clear();
    hasher_ = std::make_unique<state::StateHasher>();
}

void SoundSystem::sync() {
    if (now_ != nullptr) {
        advanceTo(now_(nowCtx_));
    }
}

void SoundSystem::ymWrite(int port, std::uint8_t value) {
    sync();
    fm_->write(port, value);
}

std::uint8_t SoundSystem::ymStatus() {
    sync();
    return fm_->readStatus();
}

void SoundSystem::psgWrite(std::uint8_t value) {
    sync();
    psg_.write(value);
}

void SoundSystem::ymReset() {
    sync();
    fm_->reset();
}

void SoundSystem::advanceTo(std::uint64_t mclk) {
    if (mclk < mclk_ + kMclkPerSample) {
        return;
    }
    const auto n = static_cast<int>((mclk - mclk_) / kMclkPerSample);
    render(n);
}

void SoundSystem::render(int n) {
    fmBuf_.resize(static_cast<std::size_t>(n) * 2);
    fm_->generate(fmBuf_.data(), n, channelMask_ & 0x7F);
    const unsigned psgMask = (channelMask_ >> 7) & 0xF;
    const auto a = static_cast<float>(1.0 - std::exp(-2.0 * std::numbers::pi * kLowPassHz / sampleRate()));
    const auto dcR = static_cast<float>(1.0 - 2.0 * std::numbers::pi * kDcBlockHz / sampleRate());
    const std::size_t base = out_.size();
    out_.resize(base + static_cast<std::size_t>(n) * 2);
    for (int i = 0; i < n; ++i) {
        const std::uint64_t end = mclk_ + kMclkPerSample;
        int psgSum = 0;
        int ticks = 0;
        while (psgNext_ < end) {
            psg_.tick();
            psgSum += psg_.mix(psgMask);
            ++ticks;
            psgNext_ += kMclkPerPsgTick;
        }
        mclk_ = end;
        // Box-filtered PSG (4-5 ticks per sample) mixed into both sides.
        const float psg =
            ticks != 0 ? static_cast<float>(psgSum) / static_cast<float>(ticks) * mix_.psgGain : 0.0f;
        float l = static_cast<float>(fmBuf_[static_cast<std::size_t>(2 * i)]) * mix_.fmGain + psg;
        float r = static_cast<float>(fmBuf_[static_cast<std::size_t>(2 * i + 1)]) * mix_.fmGain + psg;
        // DC blocker (first-order high-pass): removes the DAC's idle offset so silence is silent.
        const float hl = l - dcIn_[0] + dcR * dcOut_[0];
        const float hr = r - dcIn_[1] + dcR * dcOut_[1];
        dcIn_ = {l, r};
        dcOut_ = {hl, hr};
        l = hl;
        r = hr;
        if (mix_.lowPass) {
            lpL_ += a * (l - lpL_);
            lpR_ += a * (r - lpR_);
            l = lpL_;
            r = lpR_;
        }
        out_[base + static_cast<std::size_t>(2 * i)] = clip(l);
        out_[base + static_cast<std::size_t>(2 * i + 1)] = clip(r);
    }
    hasher_->bytes(std::span(reinterpret_cast<const std::uint8_t*>(out_.data() + base),
                             static_cast<std::size_t>(n) * 2 * sizeof(std::int16_t)));
}

void SoundSystem::hashState(state::StateHasher& h) {
    h.tag("SND ");
    h.bytes(fm_->serialize());
    const PsgState& p = psg_.state();
    for (const auto v : p.period) {
        h.u16(v);
    }
    h.bytes(p.attenuation);
    h.u8(p.noiseControl);
    h.u8(p.latch);
    for (const auto v : p.counter) {
        h.u16(v);
    }
    for (const auto v : p.output) {
        h.u8(static_cast<std::uint8_t>(v));
    }
    h.u16(p.lfsr);
    h.boolean(p.noiseFlip);
    h.u64(mclk_);
    h.u64(psgNext_);
    h.u64(audioHash());
}

void SoundSystem::serialize(state::Archive& ar) {
    std::vector<std::uint8_t> fm = ar.loading() ? std::vector<std::uint8_t>{} : fm_->serialize();
    ar.blob(fm);
    PsgState& p = psg_.state();
    ar.u16s(p.period);
    ar.bytes(p.attenuation);
    ar.u8(p.noiseControl);
    ar.u8(p.latch);
    ar.u16s(p.counter);
    for (auto& o : p.output) {
        ar.i8(o);
    }
    ar.u16(p.lfsr);
    ar.boolean(p.noiseFlip);
    ar.boolean(pal_);
    ar.u64(mclk_);
    ar.u64(psgNext_);
    ar.f32(lpL_);
    ar.f32(lpR_);
    for (float* f : {&dcIn_[0], &dcIn_[1], &dcOut_[0], &dcOut_[1]}) {
        ar.f32(*f);
    }
    hasher_->serialize(ar);
    if (ar.loading() && ar.ok()) {
        fm_ = makeYmfmSynth();
        if (fm.size() != fm_->serialize().size()) {
            ar.fail("YM2612 state has the wrong size");
            return;
        }
        fm_->deserialize(fm);
        out_.clear();
    }
}

}  // namespace gen::audio
