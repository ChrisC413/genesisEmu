# M01-04 · Data movement instructions

**Milestone:** [M01 – Motorola 68000 CPU](README.md)  
**Status:** Done  
**Depends on:** M01-03  
**Spec refs:** [02-cpu-m68k](../../docs/specs/02-cpu-m68k.md#instruction-groups-story-breakdown)

## Story

As a **developer**, I want MOVE, MOVEA, MOVEQ, MOVEM, MOVEP, LEA, PEA, EXG, SWAP, LINK, UNLK, CLR, so that the core can shuffle data like real code does.

## Scope

- All listed instructions and sizes
- MOVEM register-list order and pre-decrement reversal; MOVEM.W sign-extension into An
- MOVEP byte-lane behavior

## Acceptance criteria

- [x] 100% pass of the corresponding SingleStepTests files — MOVE.b/w/l, MOVE.q (MOVEQ), MOVEA.w/l, MOVEM.w/l, MOVEP.w/l, LEA, PEA, EXG, SWAP, LINK, UNLINK, CLR.b/w/l: 100% of 153,235 vectors (19 files) including cycles (SingleStepTests 680x0 @ e0d5ece9 (`gen_m68k_sst`, Release, 2026-10-24))

## Test plan

- SingleStepTests for each mnemonic
