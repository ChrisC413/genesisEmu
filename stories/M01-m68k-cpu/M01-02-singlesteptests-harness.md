# M01-02 · SingleStepTests 68000 harness

**Milestone:** [M01 – Motorola 68000 CPU](README.md)  
**Status:** Draft  
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

- [ ] Runner reports per-file pass rates
- [ ] A mismatch prints initial state, expected vs actual diff
- [ ] CI runs a fast subset per push; nightly runs all

## Test plan

- Self-test: a deliberately broken NOP fails the NOP vectors
