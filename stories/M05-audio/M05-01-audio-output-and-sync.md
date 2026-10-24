# M05-01 · Audio output, resampling and A/V sync

**Milestone:** [M05 – Audio](README.md)  
**Status:** Draft  
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
