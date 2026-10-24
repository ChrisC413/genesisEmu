# M10-06 · Sonic 2 mod: retry special stage after failure

**Milestone:** [M10 – Modding pipeline & first Sonic 2 mods](README.md)  
**Status:** Draft  
**Depends on:** M10-02, M10-04, M10-05  
**Spec refs:** [14-modding-pipeline](../../docs/specs/14-modding-pipeline.md#special-stage-mods-for-sonic-2)

## Story

As a **player**, I want a "Retry? Yes/No" option when I fail a special stage, so that I can try again instead of returning to the level.

## Scope

- Hook the S2 special-stage failure result path
- Prompt UI drawn with existing S2 assets/font
- Yes: restart same stage with the same entry state (rings, stage index); No: original behavior
- Build-time options per Q14 (limit retries, cost)
- Retry path shared with spliced stages later (M11)

## Acceptance criteria

- [ ] Failing stage 1 offers retry; Yes restarts stage 1; No returns to the level exactly as the original
- [ ] Mod diff shows changes only in special-stage paths
- [ ] Hardware-strict mode clean; flash-cart test passed

## Test plan

- `[rom]` scripted test failing a stage then choosing Yes/No
- Hardware checklist (M10-04)
