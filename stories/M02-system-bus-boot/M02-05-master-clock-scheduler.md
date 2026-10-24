# M02-05 · Master-clock scheduler (NTSC/PAL)

**Milestone:** [M02 – System bus & boot](README.md)  
**Status:** Draft  
**Depends on:** M02-01  
**Spec refs:** [01-architecture](../../docs/specs/01-architecture.md#timing-and-scheduler)

## Story

As a **developer**, I want a scanline scheduler driving the 68k (and later Z80/audio) by master clock, so that all components stay in sync deterministically.

## Scope

- 64-bit MCLK counter; 3420 MCLK/line; 262/313 lines
- 68k runs per scanline with overshoot carry
- Catch-up interface for Z80 and audio (no-ops for now)
- `Machine::run(RunLimit)` / `step(cpu)` and `RunResult`

## Acceptance criteria

- [ ] One NTSC frame = 896,040 MCLK
- [ ] Frame counter and line counter exposed
- [ ] `run(frames=1)` stops exactly at frame end

## Test plan

- Unit tests with a timing test ROM (synthetic)
