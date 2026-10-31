# M05-06 · Audio regression tests and WAV recording

**Milestone:** [M05 – Audio](README.md)  
**Status:** Done on Windows; macOS run pending  
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

## Notes

- `gen_cli run --wav <file>` records the native-rate mix (16-bit stereo, ~53,267 Hz). The app records from
  the Sound panel.
- `checkpoint <frame> audio_hash` hashes every native sample since power on (XXH3), so it does not depend on
  the host resampler. Each script in tests/scripts/ asserts it at the title screen and at the end of the run;
  the `[rom]` script test checks them.
- Stable across runs on Windows (recorded in one process, re-checked in another). **Not verified on macOS**,
  so the criterion stays open.
