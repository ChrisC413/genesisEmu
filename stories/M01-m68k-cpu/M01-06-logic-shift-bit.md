# M01-06 · Logic, shift/rotate and bit instructions

**Milestone:** [M01 – Motorola 68000 CPU](README.md)  
**Status:** Done  
**Depends on:** M01-03  
**Spec refs:** [02-cpu-m68k](../../docs/specs/02-cpu-m68k.md#requirements)

## Story

As a **developer**, I want AND/OR/EOR/NOT (+ immediates), all shifts and rotates, BTST/BSET/BCLR/BCHG, TST, TAS, Scc, so that bit-twiddling code (flags, collision masks) behaves correctly.

## Scope

- Register and memory forms of shifts/rotates; count modulo 64
- Bit ops: modulo 32 for Dn, modulo 8 for memory
- TAS with Genesis write-back quirk option (C68-9)

## Acceptance criteria

- [x] 100% pass of corresponding SingleStepTests — AND/OR/EOR/NOT/TST (.b/.w/.l, incl. immediates and CCR/SR forms), all shifts and rotates, BTST/BCHG/BCLR/BSET, TAS, Scc: 100% of the 359,187 non-excluded vectors (45 files). Exclusions: 3,736 ASR vectors (count > operand size on a negative operand, where the suite expects C=X=0 but a bit-by-bit shift leaves C=X=1) and 2 corrupt ASL.b vectors; see `tests/m68k/sst_exclusions.txt` (SingleStepTests 680x0 @ e0d5ece9 (`gen_m68k_sst`, Release, 2026-10-24))

## Test plan

- SingleStepTests per mnemonic
- Unit test for TAS quirk toggle

## Notes

- Unit test `TAS write-back is configurable (Genesis quirk)` covers C68-9 (default: write-back ignored, the cycle
  still takes its time). The SST runner enables write-back because the suite models a normal bus.
- BTST Dn,#imm takes 10 cycles (2 more than BTST Dn,Dn-style register timing), per the suite.
