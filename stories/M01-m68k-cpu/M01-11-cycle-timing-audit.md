# M01-11 · Cycle timing audit

**Milestone:** [M01 – Motorola 68000 CPU](README.md)  
**Status:** Draft  
**Depends on:** M01-04, M01-05, M01-06, M01-07, M01-08, M01-09, M01-10  
**Spec refs:** [02-cpu-m68k](../../docs/specs/02-cpu-m68k.md#cycle-timing-notes)

## Story

As a **developer**, I want every instruction's cycle count verified against the suite and the manual tables, so that frame timing — and therefore V-int, H-int and DMA scheduling — is right.

## Scope

- Cross-check suite cycle fields for all files
- Document any deliberate deviation

## Acceptance criteria

- [ ] Cycle mismatches across the full suite = 0 (or listed exclusions)
- [ ] Throughput benchmark: ≥ 100 MIPS-equivalent of 68k instructions/sec on the reference machine (≫ 1.3 M/s needed)

## Test plan

- Full suite run with cycle checking
- Microbenchmark in `tests/bench`
