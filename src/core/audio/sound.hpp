#pragma once
// The Genesis sound section (spec 06, M05): YM2612 (IFmSynth / ymfm) and SN76489 (Psg), advanced in catch-up
// fashion to the master clock whenever they are accessed and at the end of every scanline, mixed at the
// YM2612's native rate (MCLK / 1008, ~53,267 Hz NTSC) into interleaved stereo int16 samples.

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

#include "core/audio/fm_synth.hpp"
#include "core/audio/psg.hpp"
#include "core/state/state_hash.hpp"
#include "core/z80/z80_control.hpp"

namespace gen::audio {

inline constexpr int kMclkPerSample = kFmClockDivider * kFmSampleClocks;  // 1008
inline constexpr int kMclkPerPsgTick = 15 * 16;                           // 240
inline constexpr double kMclkNtsc = 53'693'175.0;
inline constexpr double kMclkPal = 53'203'424.0;

// Channel bits for mute / solo (spec 06 A-1).
enum ChannelBit : unsigned {
    kChFm1 = 1u << 0,  // FM 1-6 = bits 0-5
    kChDac = 1u << 6,
    kChPsg1 = 1u << 7,  // PSG tone 1-3 = bits 7-9
    kChNoise = 1u << 10,
    kChAll = (1u << 11) - 1,
};
inline constexpr int kChannelCount = 11;
const char* channelName(int bit);

struct MixSettings {
    float fmGain = 1.0f;
    float psgGain = 1.0f;
    bool lowPass = true;  // ~3.4 kHz first-order low-pass of a model 1 console
};

class SoundSystem final : public z80::SoundPorts {
public:
    SoundSystem();
    ~SoundSystem() override;

    void reset(bool pal);
    // Current master clock of the accessing CPU; the chips catch up to it before every access.
    void setClockSource(std::uint64_t (*now)(void* ctx), void* ctx) {
        now_ = now;
        nowCtx_ = ctx;
    }

    // z80::SoundPorts
    void ymWrite(int port, std::uint8_t value) override;
    std::uint8_t ymStatus() override;
    void psgWrite(std::uint8_t value) override;
    void ymReset() override;

    // Renders everything up to `mclk` (no-op if already there).
    void advanceTo(std::uint64_t mclk);

    // Samples rendered since the last clear (interleaved L/R, native rate).
    const std::vector<std::int16_t>& samples() const { return out_; }
    void clearSamples() { out_.clear(); }
    double sampleRate() const { return (pal_ ? kMclkPal : kMclkNtsc) / kMclkPerSample; }

    // Hash of every sample rendered since reset (regression checkpoints, M05-06). Mutes and mix settings
    // change it; tests use the defaults.
    std::uint64_t audioHash() const { return hasher_->digest(); }

    void setChannelMask(unsigned mask) { channelMask_ = mask; }
    unsigned channelMask() const { return channelMask_; }
    MixSettings& mix() { return mix_; }

    IFmSynth& fm() { return *fm_; }
    Psg& psg() { return psg_; }
    void hashState(state::StateHasher& h);
    // Save states: both chips, the catch-up clocks, the output filters and the running audio hash. Pending
    // samples() are dropped on load; mutes and mix settings are frontend settings and are not saved.
    void serialize(state::Archive& ar);

private:
    void sync();
    void render(int n);

    std::unique_ptr<IFmSynth> fm_;
    Psg psg_;
    std::uint64_t (*now_)(void*) = nullptr;
    void* nowCtx_ = nullptr;
    bool pal_ = false;
    std::uint64_t mclk_ = 0;     // rendered up to here
    std::uint64_t psgNext_ = 0;  // master clock of the next PSG tick
    float lpL_ = 0;
    float lpR_ = 0;
    std::array<float, 2> dcIn_{};  // DC blocker state (the YM2612 DAC idles at a positive offset)
    std::array<float, 2> dcOut_{};
    unsigned channelMask_ = kChAll;
    MixSettings mix_;
    std::vector<std::int16_t> out_;
    std::vector<std::int32_t> fmBuf_;
    std::unique_ptr<state::StateHasher> hasher_;
};

}  // namespace gen::audio
