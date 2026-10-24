# M07-06 · Z80 code provenance back to ROM

**Milestone:** [M07 – ROM & memory exploration](README.md)  
**Status:** Draft  
**Depends on:** M07-01  
**Spec refs:** [09-coverage-and-rom-map](../../docs/specs/09-coverage-and-rom-map.md#provenance-z80-code--rom)

## Story

As a **reverse engineer**, I want Z80 code executed from Z80 RAM attributed back to the ROM bytes it came from, so that sound-driver code appears on the ROM map.

## Scope

- Track 68k writes into Z80 RAM and match to recent ROM reads
- Confidence levels (direct copy vs decompressed block)
- Z80 RAM exec counts projected to ROM

## Acceptance criteria

- [ ] Sonic 1 DAC driver shows as Z80 code (high confidence) at its ROM location
- [ ] Sonic 2 compressed driver block marked Z80 (low confidence)

## Test plan

- `[rom]` tests
