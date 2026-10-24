# M01-02 · SingleStepTests 68000 harness

**Milestone:** [M01 – Motorola 68000 CPU](README.md)  
**Status:** Done  
**Depends on:** M01-01  
**Spec refs:** [02-cpu-m68k](../../docs/specs/02-cpu-m68k.md), [13-testing-strategy](../../docs/specs/13-testing-strategy.md#layers)

## Story

As a **developer**, I want a test runner that loads the SingleStepTests JSON vectors and compares full CPU + memory state, so that every instruction group is verified the moment it's written.

## Scope

- `tools/fetch-test-data` downloads the suite pinned by commit + checksum
- Runner: set initial state (regs, prefetch, RAM), run one instruction, compare final regs/RAM/cycles
- Filter by mnemonic file; summary table of pass/fail per instruction
- Exclusion list file with a reason per entry

## Acceptance criteria

- [x] Runner reports per-file pass rates — one line per file (pass / total / state mismatches / cycle-only mismatches / excluded) and a total
- [x] A mismatch prints initial state, expected vs actual diff — `--show N` prints the initial registers and every differing field (`--failures FILE` logs all)
- [x] CI runs a fast subset per push; nightly runs all — the Debug job runs the first 250 vectors of every file; the Release job runs **all** 1,000,060 per push (about 3 s), so no separate nightly workflow was needed. Vectors are cached by the lock-file hash

## Test plan

- Self-test: a deliberately broken NOP fails the NOP vectors

## Notes

- `python tools/fetch-test-data.py --only m68k` downloads the suite (124 files, 202.6 MB of .json.gz) pinned to
  commit `e0d5ece9`, checks each file's git blob SHA-1 from `tools/test-data.lock.json`, and converts it to the
  compact `.sst` format (`tests/m68k/sst_format.hpp`, ~250 MB in `test-data/m68k`). Re-running skips files that
  are up to date.
- Self-test (`m68k.sst_selftest`): `--inject-fault` corrupts D0 after every NOP; the run must report failures.
- The suite has no LICENSE file; it is fetched for local testing only and never vendored.
- All vectors start in supervisor mode with interrupt mask 7, SSP = $800 and T = 0, so user mode, trace and
  interrupts are covered by unit tests instead.
