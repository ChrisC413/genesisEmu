# M02-06 · Instruction trace logger (CLI)

**Milestone:** [M02 – System bus & boot](README.md)  
**Status:** Draft  
**Depends on:** M01-12, M02-05  
**Spec refs:** [08-debugger](../../docs/specs/08-debugger.md#tracing-trace-from-pause-d-007), [13-testing-strategy](../../docs/specs/13-testing-strategy.md#reference-comparison-debug-aid)

## Story

As a **developer**, I want `gen_cli trace` writing a text trace of 68k instructions with register deltas, so that I can diff bring-up behavior against a reference emulator.

## Scope

- Text format per spec 08 tracing section
- Frame range / PC range filters
- `tools/trace-diff` reporting first divergence

## Acceptance criteria

- [ ] Trace of frame 0–10 of Sonic 1 is produced
- [ ] Format documented in `docs/specs/08-debugger.md` (trace section)
- [ ] trace-diff finds an injected divergence

## Test plan

- Golden trace for a synthetic ROM

## Notes

The interactive trace-from-pause UI comes in M06-09 and reuses this writer.
