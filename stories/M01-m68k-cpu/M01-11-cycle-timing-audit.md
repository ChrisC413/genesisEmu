# M01-11 · Cycle timing audit

**Milestone:** [M01 – Motorola 68000 CPU](README.md)  
**Status:** In progress  
**Depends on:** M01-04, M01-05, M01-06, M01-07, M01-08, M01-09, M01-10  
**Spec refs:** [02-cpu-m68k](../../docs/specs/02-cpu-m68k.md#cycle-timing-notes)

## Story

As a **developer**, I want every instruction's cycle count verified against the suite and the manual tables, so that frame timing — and therefore V-int, H-int and DMA scheduling — is right.

## Scope

- Cross-check suite cycle fields for all files
- Document any deliberate deviation

## Acceptance criteria

- [x] Cycle mismatches across the full suite = 0 (or listed exclusions) — 996,322 / 996,322 non-excluded vectors pass with exact cycle counts; 3,738 excluded vectors are listed with reasons in `tests/m68k/sst_exclusions.txt` (SingleStepTests 680x0 @ e0d5ece9 (`gen_m68k_sst`, Release, 2026-10-24))
- [ ] Throughput benchmark: ≥ 100 MIPS-equivalent of 68k instructions/sec on the reference machine (≫ 1.3 M/s needed) — *not met yet*: `gen_m68k_bench 100` gives 95 MIPS (≈110× a real 68000) on an i7-11700KF (Release, MSVC). Not profiled yet; candidates are the virtual bus call per access and the per-step interrupt/hook checks

## Test plan

- Full suite run with cycle checking
- Microbenchmark in `tests/bench`

## Exclusions

| Vectors | Reason |
|---------|--------|
| 3,736 in ASR.b/w/l | Register count > operand size on a negative operand: the suite expects C = X = 0. A bit-by-bit shift keeps shifting out the sign bit, so the PRM ("C: set according to the last bit shifted out") and Musashi give C = X = 1. The core follows the PRM. |
| 2 in ASL.b | `e502 [ASL.b Q, D2] 1583` and `1761`: the expected final D2 differs in all 32 bits, which a byte shift cannot do (corrupt vectors). |

## Notes

Deliberate choices where the suite was the only reference are listed in M01-05, M01-08 and M01-10.
