# M01-08 · Program control instructions

**Milestone:** [M01 – Motorola 68000 CPU](README.md)  
**Status:** Done  
**Depends on:** M01-03  
**Spec refs:** [02-cpu-m68k](../../docs/specs/02-cpu-m68k.md)

## Story

As a **developer**, I want Bcc, BRA, BSR, DBcc, JMP, JSR, RTS, RTR, RTE, NOP, so that control flow — and the debugger's step-over logic — works.

## Scope

- Short/word branch displacements; taken/not-taken timing
- DBcc counter wrap at -1
- RTE format for 68000 (SR + PC)

## Acceptance criteria

- [x] 100% pass of corresponding SingleStepTests — Bcc, BSR, DBcc, JMP, JSR, RTS, RTR, RTE, NOP: 100% of 72,585 vectors including odd-target address errors (SingleStepTests 680x0 @ e0d5ece9 (`gen_m68k_sst`, Release, 2026-10-24))
- [x] Flow type (call/return/branch/jump) exposed for each executed instruction — `M68k::lastFlow()` (Sequential, Branch, Jump, Call, Return, Exception, Stopped); unit test `Flow type is reported for control transfers`

## Test plan

- SingleStepTests

## Notes

A fault on the first fetch at a jump target stacks target - 4 (and the target as the access address), per the
suite; JSR fetches the first target word before pushing the return address, so an odd target leaves the stack
untouched.
