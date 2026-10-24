# M11-07 · RAM and VRAM remapping planner

**Milestone:** [M11 – Special-stage splicing](README.md)  
**Status:** Draft  
**Depends on:** M11-03  
**Spec refs:** [15-subsystem-splicing](../../docs/specs/15-subsystem-splicing.md#4-splice-plan-target-sonic-2)

## Story

As a **reverse engineer**, I want an automatic RAM map from donor addresses to target RAM with conflict detection against target RAM live during special stages, so that the spliced stage can't corrupt the host game's state.

## Scope

- Scratch → target dead-during-special-stage RAM
- Inputs/outputs → target equivalents via glue
- Conflict check using target coverage
- Editable table; VDP entry-state requirements

## Acceptance criteria

- [ ] Plan for S1 stage in S2 has no conflicts; deliberately overlapping mapping is flagged

## Test plan

- Unit tests; `[rom]` conflict check
