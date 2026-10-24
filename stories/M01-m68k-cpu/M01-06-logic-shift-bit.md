# M01-06 · Logic, shift/rotate and bit instructions

**Milestone:** [M01 – Motorola 68000 CPU](README.md)  
**Status:** Draft  
**Depends on:** M01-03  
**Spec refs:** [02-cpu-m68k](../../docs/specs/02-cpu-m68k.md#requirements)

## Story

As a **developer**, I want AND/OR/EOR/NOT (+ immediates), all shifts and rotates, BTST/BSET/BCLR/BCHG, TST, TAS, Scc, so that bit-twiddling code (flags, collision masks) behaves correctly.

## Scope

- Register and memory forms of shifts/rotates; count modulo 64
- Bit ops: modulo 32 for Dn, modulo 8 for memory
- TAS with Genesis write-back quirk option (C68-9)

## Acceptance criteria

- [ ] 100% pass of corresponding SingleStepTests

## Test plan

- SingleStepTests per mnemonic
- Unit test for TAS quirk toggle
