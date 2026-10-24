# M11-12 · Second splice: S3K Blue Sphere into Sonic 2

**Milestone:** [M11 – Special-stage splicing](README.md)  
**Status:** Draft  
**Depends on:** M11-11  
**Spec refs:** [15-subsystem-splicing](../../docs/specs/15-subsystem-splicing.md)

## Story

As a **player**, I want Blue Sphere stages inside Sonic 2, so that the pipeline generalizes beyond one donor.

## Scope

- S3K donor project analysis of Blue Sphere
- Z80 driver/sound mapping for S3K sounds
- Larger ROM placement (cart size check)

## Acceptance criteria

- [ ] Same acceptance as M11-11 for Blue Sphere
- [ ] Pipeline changes needed are captured as fixes to M11 tools, not one-off hacks

## Test plan

- `[rom]` end-to-end scripted test
- Hardware checklist
