#include "app/audio_output.hpp"

#include <algorithm>
#include <cmath>

namespace gen::app {

namespace {
constexpr double kTargetSeconds = 2.0 / 60.0;  // ~two frames queued
constexpr double kMaxQueueSeconds = 0.25;      // beyond this, drop instead of growing the latency
}  // namespace

AudioOutput::~AudioOutput() {
    if (stream_ != nullptr) {
        SDL_DestroyAudioStream(stream_);
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
    }
}

bool AudioOutput::open(std::string& error) {
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        error = std::string("SDL audio init failed: ") + SDL_GetError();
        return false;
    }
    const SDL_AudioSpec spec{SDL_AUDIO_S16, 2, kRate};
    stream_ = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);
    if (stream_ == nullptr) {
        error = std::string("cannot open the audio device: ") + SDL_GetError();
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        return false;
    }
    SDL_ResumeAudioStreamDevice(stream_);
    return true;
}

void AudioOutput::setSourceRate(double rate) {
    if (rate != sourceRate_) {
        sourceRate_ = rate;
        resampler_.setRates(rate, kRate);
    }
    if (stream_ != nullptr) {
        SDL_ClearAudioStream(stream_);
    }
}

int AudioOutput::queuedMs() const {
    if (stream_ == nullptr) {
        return 0;
    }
    const int bytes = SDL_GetAudioStreamQueued(stream_);
    return bytes * 1000 / (kRate * 4);
}

void AudioOutput::setPaused(bool paused) {
    if (paused == paused_ || stream_ == nullptr) {
        paused_ = paused;
        return;
    }
    paused_ = paused;
    if (paused) {
        SDL_ClearAudioStream(stream_);  // no buzzing of the last buffer
        SDL_PauseAudioStreamDevice(stream_);
    } else {
        resampler_.reset();
        SDL_ResumeAudioStreamDevice(stream_);
    }
}

void AudioOutput::push(std::span<const std::int16_t> samples) {
    if (stream_ == nullptr || paused_ || sourceRate_ <= 0) {
        return;
    }
    if (muted_) {
        SDL_ClearAudioStream(stream_);
        return;
    }
    // Dynamic rate control: a fuller queue than the target consumes input faster (fewer output samples).
    const double queued = static_cast<double>(SDL_GetAudioStreamQueued(stream_)) / (kRate * 4.0);
    if (queued > kMaxQueueSeconds) {
        SDL_ClearAudioStream(stream_);
    }
    const double error = (queued - kTargetSeconds) / kTargetSeconds;
    resampler_.setRatioAdjust(1.0 + std::clamp(error * 0.005, -0.005, 0.005));
    buf_.clear();
    resampler_.process(samples, buf_);
    if (volume_ != 1.0f) {
        for (auto& s : buf_) {
            s = static_cast<std::int16_t>(std::clamp(std::lround(s * volume_), -32768L, 32767L));
        }
    }
    SDL_PutAudioStreamData(stream_, buf_.data(), static_cast<int>(buf_.size() * sizeof(std::int16_t)));
}

}  // namespace gen::app
