# M02-06 · Instruction trace logger (CLI)

**Milestone:** [M02 – System bus & boot](README.md)  
**Status:** Done  
**Depends on:** M01-12, M02-05  
**Spec refs:** [08-debugger](../../docs/specs/08-debugger.md#tracing-trace-from-pause-d-007), [13-testing-strategy](../../docs/specs/13-testing-strategy.md#reference-comparison-debug-aid)

## Story

As a **developer**, I want `gen_cli trace` writing a text trace of 68k instructions with register deltas, so that I can diff bring-up behavior against a reference emulator.

## Scope

- Text format per spec 08 tracing section
- Frame range / PC range filters
- `tools/trace-diff` reporting first divergence

## Acceptance criteria

- [x] Trace of frame 0–10 of Sonic 1 is produced — `gen_cli trace --rom <sonic1> --frames 11 --out s1.txt`: 163,613 records (18 MB) in 0.16 s; not committed (commercial code)
- [x] Format documented in `docs/specs/08-debugger.md` (trace section) — "Text trace format (v1)"
- [x] trace-diff finds an injected divergence — `cli.trace_golden` changes one register delta in a copy of the golden trace and expects `tools/trace-diff.py` to report it (runs when CMake finds Python 3)

## Test plan

- Golden trace for a synthetic ROM

## Notes

The interactive trace-from-pause UI comes in M06-09 and reuses this writer.

- Writer: `src/instrument/trace_writer.{hpp,cpp}` (in `gen_instrument`, so the disassembler is injected as a callback by `gen_cli`). It attaches through the normal `Hooks`; with no writer attached the core only pays its existing null-pointer checks.
- Golden: `tests/golden/boot_trace.txt`, the `--first` trace of three frames of the synthetic boot ROM, compared byte for byte.
- Not yet: the binary trace format and ring-buffer mode (M06-09).
