# M05-04 · YM2612 DAC and timers

**Milestone:** [M05 – Audio](README.md)  
**Status:** Implemented; listening checks pending (Chris)  
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

## Notes

- DAC (`$2A` data, `$2B` enable) replaces channel 6 (ymfm), with the DAC discontinuity; a DC blocker in the
  mixer removes the idle DAC offset so silence is silent.
- Timers A/B and the busy flag: ymfm's timer callbacks are counted down by 144 YM clocks per generated
  sample; status reads catch the chips up to the reading CPU's clock first. Unit tests cover the timer A
  flag and the DAC.
- Objective checks: in Sonic 1 the "SEGA" chant is the only sound during the logo (2.5-4.5 s after power
  on, -23 dBFS, zero-crossing rate ~1.3 kHz as expected for speech) and the register log shows 27,000 Z80
  writes to `$2A` in that window; Sonic 2 and S3K play their SEGA samples at ~4 s and ~1-2 s.
- **Not verified:** "SEGA voice clear and correct pitch" and "S3K DAC drums correct" (listening).
