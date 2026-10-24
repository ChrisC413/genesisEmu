# M11-02 · Subsystem definition and closure

**Milestone:** [M11 – Special-stage splicing](README.md)  
**Status:** Draft  
**Depends on:** M11-01  
**Spec refs:** [15-subsystem-splicing](../../docs/specs/15-subsystem-splicing.md#1-define-the-subsystem-donor-project)

## Story

As a **reverse engineer**, I want to define a subsystem from a watch layer and root entries and get its closure of code and data, with owned/shared classification, so that I know exactly what a special stage consists of.

## Scope

- Seed from watch layer (enter → results)
- Roots: game-mode entry + interrupt paths
- Closure over static call graph + observed code + referenced data
- Owned vs shared classification
- Footprint overlay on the ROM map

## Acceptance criteria

- [ ] Sonic 1 special stage subsystem defined; spot-check of members against s1disasm labels
- [ ] Shared routines (decompressors, DMA queue, PlaySound) classified as shared

## Test plan

- `[rom]` test with a scripted special-stage run
