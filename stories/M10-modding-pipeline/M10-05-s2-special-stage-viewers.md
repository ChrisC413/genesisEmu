# M10-05 · Sonic 2 special-stage data viewers

**Milestone:** [M10 – Modding pipeline & first Sonic 2 mods](README.md)  
**Status:** Draft  
**Depends on:** M10-01  
**Spec refs:** [15-subsystem-splicing](../../docs/specs/15-subsystem-splicing.md#6-tooling-for-s2-special-stage-modifications)

## Story

As a **reverse engineer**, I want viewers for Sonic 2 special-stage data: layout segments, ring/bomb placement, checkpoint ring requirements, stage order, so that I can understand and modify the native stages.

## Scope

- Data types for S2 special-stage formats (spec 10 typing)
- Visual preview of segments and object placement
- Stage table view (order, requirements)

## Acceptance criteria

- [ ] All seven S2 stages viewable; requirements match in-game values

## Test plan

- `[rom]` checks against reference disassembly data labels
