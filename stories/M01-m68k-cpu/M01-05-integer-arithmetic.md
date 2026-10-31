# M01-05 · Integer arithmetic instructions

**Milestone:** [M01 – Motorola 68000 CPU](README.md)  
**Status:** Done  
**Depends on:** M01-03  
**Spec refs:** [02-cpu-m68k](../../docs/specs/02-cpu-m68k.md#cycle-timing-notes)

## Story

As a **developer**, I want ADD/SUB families, CMP family, MULU/MULS, DIVU/DIVS, NEG/NEGX, EXT, so that game math and comparisons are exact.

## Scope

- X-flag semantics for ADDX/SUBX/NEGX (Z only cleared, never set)
- Data-dependent MUL/DIV timing
- DIV overflow behavior and zero-divide exception hook

## Acceptance criteria

- [x] 100% pass of corresponding SingleStepTests including cycle counts — ADD/ADDA/ADDX/SUB/SUBA/SUBX/CMP/CMPA/NEG/NEGX (.b/.w/.l; the suite files these with their I/Q/M forms), EXT.w/l, MULU, MULS, DIVU, DIVS: 100% of 266,145 vectors (33 files) including the data-dependent MUL/DIV cycle counts (SingleStepTests 680x0 @ e0d5ece9 (`gen_m68k_sst`, Release, 2026-10-24))

## Test plan

- SingleStepTests per mnemonic

## Notes

Behavior taken from the suite where the manual leaves flags undefined: DIVU/DIVS overflow sets V, clears C and
leaves N and Z unchanged; DIVS detects overflow up front when |quotient| >= $8000 (so -32768 also overflows) and
then takes (8 or 9) x 2 cycles. The suite has a single zero-divide vector (`DIVU (d16, A7)`); it stacks the address
of the DIV instruction itself and takes 38 cycles plus EA time, and the core follows it. ADDQ.L/SUBQ.L #,An takes 6
cycles (the suite), not 8.
