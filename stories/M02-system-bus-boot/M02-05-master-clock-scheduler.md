# M02-05 · Master-clock scheduler (NTSC/PAL)

**Milestone:** [M02 – System bus & boot](README.md)  
**Status:** Done  
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

- [x] One NTSC frame = 896,040 MCLK — `kMclkPerFrameNtsc` and a scheduler test
- [x] Frame counter and line counter exposed — `Machine::frame()`, `line()`, `lineStartMclk()`, `cpuMclk()`
- [x] `run(frames=1)` stops exactly at frame end — line 0 of the next frame at MCLK 896,040; the 68k overshoot carries over

## Test plan

- Unit tests with a timing test ROM (synthetic)

## Notes

- `src/core/sched/machine.{hpp,cpp}`: `Machine::run(RunLimit)` (frames, instructions, MCLK, until break) and `step(CpuId)`; `RunResult` gives the stop reason (limit, breakpoint, exception with `stopOnFault`, unsupported for the Z80) and the instruction count. Breakpoints stop before the instruction and are resumable.
- Per line: the 68k runs until its MCLK reaches the line end, then `catchUp()` (empty hook for the Z80 and audio), then the VDP line event and IPL update. `onFrameEnd` gets the number of the frame that ended.
- The machine also keeps cheap stats (exceptions per vector, faults, V-ints per frame, PC interrupted by the last V-int) used by the boot tests and `gen_cli run`.
- PAL (313 lines) is implemented but only lightly tested (D-020).
