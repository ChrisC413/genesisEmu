# M05-05 · Mixer, channel mute/solo and register-write log

**Milestone:** [M05 – Audio](README.md)  
**Status:** Draft  
**Depends on:** M05-02, M05-04  
**Spec refs:** [06-audio](../../docs/specs/06-audio.md#analysis-features)

## Story

As a **reverse engineer**, I want per-channel mute/solo and a log of every sound-chip register write with PC, so that I can reverse engineer the SMPS drivers.

## Scope

- FM/PSG balance and low-pass options
- Mute/solo per channel
- Ring buffer of `(frame, line, cpu, pc, chip, port, reg, value)`; CSV export
- Audio panel in the UI

## Acceptance criteria

- [ ] Muting FM6 silences DAC
- [ ] Log export of 10 s of title music contains expected key-on writes with Z80 PCs

## Test plan

- Unit tests for log; manual UI test
