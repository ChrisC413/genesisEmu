# M01-08 · Program control instructions

**Milestone:** [M01 – Motorola 68000 CPU](README.md)  
**Status:** Draft  
**Depends on:** M01-03  
**Spec refs:** [02-cpu-m68k](../../docs/specs/02-cpu-m68k.md)

## Story

As a **developer**, I want Bcc, BRA, BSR, DBcc, JMP, JSR, RTS, RTR, RTE, NOP, so that control flow — and the debugger's step-over logic — works.

## Scope

- Short/word branch displacements; taken/not-taken timing
- DBcc counter wrap at -1
- RTE format for 68000 (SR + PC)

## Acceptance criteria

- [ ] 100% pass of corresponding SingleStepTests
- [ ] Flow type (call/return/branch/jump) exposed for each executed instruction (used by the debugger and coverage)

## Test plan

- SingleStepTests
