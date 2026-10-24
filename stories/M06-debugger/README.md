# M06 · Debugger & freeze execution

**Goal:** Freeze at any instruction, step, break on anything, inspect and edit all state, save/load states, and trace from a paused state.

**Depends on:** M02 (core stories M06-01..06, 08, 09, 10), M03/M04/M05 for the VDP/Z80/audio panels

## Testable stage (exit criteria)

In the app: pause Sonic mid-level, step into/over/out on the 68k and Z80, set exec/memory/VDP breakpoints with conditions, inspect every memory and VDP structure, save and reload a state (determinism check passes), and record a trace started from the paused state.

## Notes

Start M06-01, M06-02, M06-05, M06-06 right after M02 to help bring up M03–M05.

## Stories

| ID | Story | Depends on |
|----|-------|------------|
| M06-01 | [Run control: pause, resume, step frame/scanline, reset](M06-01-run-control.md) | — |
| M06-02 | [Instruction stepping: into, over, out, run to cursor](M06-02-instruction-stepping.md) | M06-01, M06-10 |
| M06-03 | [Execution breakpoints, conditions and logpoints](M06-03-execution-breakpoints.md) | M06-01 |
| M06-04 | [Memory, VDP and interrupt watchpoints](M06-04-watchpoints-memory-and-io.md) | M06-03 |
| M06-05 | [CPU register panels](M06-05-cpu-register-views.md) | M06-01 |
| M06-06 | [Memory viewers / hex editor](M06-06-memory-viewers.md) | M06-01 |
| M06-07 | [VDP viewers: registers, tiles, planes, sprites, palette](M06-07-vdp-viewers.md) | M06-01 |
| M06-08 | [Save states](M06-08-save-states.md) | M06-01 |
| M06-09 | [Trace from pause, ring buffer and trace on hit](M06-09-trace-from-pause.md) | M06-03 |
| M06-10 | [Live disassembly view](M06-10-live-disassembly-view.md) | M06-01 |
| M06-11 | [Call stack view](M06-11-call-stack-view.md) | M06-02 |
