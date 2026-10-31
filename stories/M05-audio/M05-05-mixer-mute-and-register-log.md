# M05-05 · Mixer, channel mute/solo and register-write log

**Milestone:** [M05 – Audio](README.md)  
**Status:** Done  
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

- [x] Muting FM6 silences DAC
- [x] Log export of 10 s of title music contains expected key-on writes with Z80 PCs

## Test plan

- Unit tests for log; manual UI test

## Notes

- Sound panel (Emulation > Sound...): output volume, FM / PSG levels, model 1 low-pass toggle, mute and solo
  for FM 1-6, DAC, PSG 1-3 and noise, register log table (newest first) with Clear and Export CSV (to the
  ROM's user-data folder), WAV recording, DRC status.
- Muting FM 6 also silences the DAC, which replaces channel 6 (unit test).
- Register log CSV of Sonic 2's title (frames 540-1140, `gen_cli run --reg-log`): 226 key-on/off writes to
  `$28`, all from the Z80 at PC `$0021` (the driver's FM write routine), values like `$01/$02/$04` for
  channel key-offs.
