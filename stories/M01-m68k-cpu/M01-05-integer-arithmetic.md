# M01-05 · Integer arithmetic instructions

**Milestone:** [M01 – Motorola 68000 CPU](README.md)  
**Status:** Draft  
**Depends on:** M01-03  
**Spec refs:** [02-cpu-m68k](../../docs/specs/02-cpu-m68k.md#cycle-timing-notes)

## Story

As a **developer**, I want ADD/SUB families, CMP family, MULU/MULS, DIVU/DIVS, NEG/NEGX, EXT, so that game math and comparisons are exact.

## Scope

- X-flag semantics for ADDX/SUBX/NEGX (Z only cleared, never set)
- Data-dependent MUL/DIV timing
- DIV overflow behavior and zero-divide exception hook

## Acceptance criteria

- [ ] 100% pass of corresponding SingleStepTests including cycle counts

## Test plan

- SingleStepTests per mnemonic
