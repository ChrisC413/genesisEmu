#pragma once
// Host audio output (spec 06 "Mixing and output", M05-01): the native-rate mix is resampled to 48 kHz and
// pushed into an SDL_AudioStream. Dynamic rate control nudges the resampling ratio by up to +-0.5% to keep
// about two frames of audio queued. Paused: the queue is flushed and the device paused (silence, no buzz).
// Fast-forward: muted.

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include <SDL3/SDL.h>

#include "core/audio/resampler.hpp"

namespace gen::app {

class AudioOutput {
public:
    static constexpr int kRate = 48000;

    AudioOutput() = default;
    ~AudioOutput();
    AudioOutput(const AudioOutput&) = delete;
    AudioOutput& operator=(const AudioOutput&) = delete;

    // Opens the default playback device; false (logged, emulation continues silently) when unavailable.
    bool open(std::string& error);
    bool isOpen() const { return stream_ != nullptr; }

    // Called whenever the source rate changes (ROM start, region).
    void setSourceRate(double rate);
    // Queues one emulated frame of native-rate stereo samples.
    void push(std::span<const std::int16_t> samples);
    // Paused / fast-forward / normal.
    void setPaused(bool paused);
    // Drops queued audio and the resampler history (after loading a save state).
    void flush();
    void setMuted(bool muted) { muted_ = muted; }
    void setVolume(float v) { volume_ = v; }

    double ratioAdjust() const { return resampler_.ratioAdjust(); }
    int queuedMs() const;

private:
    SDL_AudioStream* stream_ = nullptr;
    audio::Resampler resampler_;
    std::vector<std::int16_t> buf_;
    double sourceRate_ = 0;
    bool paused_ = false;
    bool muted_ = false;
    float volume_ = 1.0f;
};

}  // namespace gen::app
