# M10-07 · Sonic 2 mod: data-driven special-stage modifications

**Milestone:** [M10 – Modding pipeline & first Sonic 2 mods](README.md)  
**Status:** Draft  
**Depends on:** M10-05  
**Spec refs:** [14-modding-pipeline](../../docs/specs/14-modding-pipeline.md#special-stage-mods-for-sonic-2)

## Story

As a **reverse engineer**, I want build-time options to change S2 special-stage parameters (ring requirements, stage order, selected layout/object changes), so that native stages can be rebalanced or remixed.

## Scope

- Parameter table in the generated source
- Validation that layouts/objects remain within format limits
- Each tweak toggleable

## Acceptance criteria

- [ ] Changing checkpoint ring requirements is reflected in-game
- [ ] Default parameters build byte-identical to the original

## Test plan

- `[rom]` scripted tests
