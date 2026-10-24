# M05-04 · YM2612 DAC and timers

**Milestone:** [M05 – Audio](README.md)  
**Status:** Draft  
**Depends on:** M05-03  
**Spec refs:** [06-audio](../../docs/specs/06-audio.md#ym2612-fm)

## Story

As a **developer**, I want DAC playback on channel 6 and timers A/B, so that drums, voice clips and timer-driven drivers work.

## Scope

- DAC enable/data
- Timer A/B overflow flags and reload
- Accurate-enough DAC timing from Z80 write cadence

## Acceptance criteria

- [ ] "SEGA" voice in Sonic 1 clear and correct pitch
- [ ] S3K DAC drums correct

## Test plan

- Manual listening + WAV test
