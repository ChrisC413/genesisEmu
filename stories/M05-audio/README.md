# M05 · Audio

**Goal:** PSG and YM2612 synthesis, mixing, resampling and audio/video sync, plus analysis aids (mute/solo, register log).

**Depends on:** M04

## Testable stage (exit criteria)

All three games' music, SFX and DAC samples (incl. the "SEGA" voice) sound correct at 48 kHz without crackles or drift over a 30-minute session; per-channel mute works; register log exports CSV.

## Notes

YM2612 uses ymfm (D-013); the PSG is in-house.

## Stories

| ID | Story | Depends on |
|----|-------|------------|
| M05-01 | [Audio output, resampling and A/V sync](M05-01-audio-output-and-sync.md) | — |
| M05-02 | [PSG (SN76489)](M05-02-psg-sn76489.md) | M05-01 |
| M05-03 | [YM2612 FM synthesis (ymfm)](M05-03-ym2612-fm.md) | M05-01 |
| M05-04 | [YM2612 DAC and timers](M05-04-ym2612-dac-and-timers.md) | M05-03 |
| M05-05 | [Mixer, channel mute/solo and register-write log](M05-05-mixer-mute-and-register-log.md) | M05-02, M05-04 |
| M05-06 | [Audio regression tests and WAV recording](M05-06-audio-regression-tests.md) | M05-05, M04-03 |
