# M05-01 · Audio output, resampling and A/V sync

**Milestone:** [M05 – Audio](README.md)  
**Status:** Implemented; listening / long-run checks pending (Chris)  
**Depends on:** —  
**Spec refs:** [06-audio](../../docs/specs/06-audio.md#mixing-and-output)

## Story

As a **player**, I want glitch-free audio synchronized with video, so that the game sounds right for long sessions.

## Scope

- SDL_AudioStream output at 48 kHz
- Band-limited resampler
- Dynamic rate control ±0.5% around a fill target
- Silence when paused; mute on fast-forward

## Acceptance criteria

- [ ] No underruns over 30 minutes at 60 Hz and at 144 Hz host refresh
- [ ] Pause produces silence immediately

## Test plan

- Long-run soak test with fill-level logging

## Notes

- `audio::Resampler` (src/core/audio/resampler.*): 32-tap Blackman-windowed sinc, 256 interpolated phases,
  cutoff at 90% of the lower Nyquist rate. Unit test: a 1 kHz tone keeps its pitch within 0.5% and its level
  within 2% from 53,267 Hz to 48 kHz.
- `app::AudioOutput`: SDL3 `SDL_AudioStream` at 48 kHz S16 stereo. Dynamic rate control adjusts the ratio
  by up to +-0.5% to keep ~2 frames (33 ms) queued, and drops the queue above 250 ms. Paused: the stream is
  cleared and the device paused. Fast-forward: muted. The app runs silently with a logged warning when no
  audio device opens (CI runners).
- **Not verified:** 30-minute underrun runs at 60 Hz and 144 Hz (the build machine's display runs at
  100 Hz; there is no unattended way to listen for underruns), and "pause is silent immediately" by ear.
