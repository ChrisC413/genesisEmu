# M01-07 · BCD instructions

**Milestone:** [M01 – Motorola 68000 CPU](README.md)  
**Status:** Done  
**Depends on:** M01-03  
**Spec refs:** [02-cpu-m68k](../../docs/specs/02-cpu-m68k.md)

## Story

As a **developer**, I want ABCD, SBCD, NBCD including undefined-flag behavior as the test suite expects, so that score/timer display code is correct.

## Scope

- Register and -(An) forms
- N and V flag behavior per real silicon

## Acceptance criteria

- [x] 100% pass of SingleStepTests ABCD/SBCD/NBCD — ABCD, SBCD, NBCD: 100% of 24,195 vectors including the undefined N and V flags (SingleStepTests 680x0 @ e0d5ece9 (`gen_m68k_sst`, Release, 2026-10-24))

## Test plan

- SingleStepTests
