# M05-06 · Audio regression tests and WAV recording

**Milestone:** [M05 – Audio](README.md)  
**Status:** Draft  
**Depends on:** M05-05, M04-03  
**Spec refs:** [06-audio](../../docs/specs/06-audio.md#testing)

## Story

As a **developer**, I want WAV recording and audio-hash checkpoints in scripted tests, so that audio regressions are caught.

## Scope

- WAV writer of mixed output
- Audio hash checkpoint in the script format

## Acceptance criteria

- [ ] Title-screen audio hash stable across runs and OSes

## Test plan

- `[rom]` CTest
