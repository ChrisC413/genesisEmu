# M11-08 · Game-mode, interrupt and result glue generation

**Milestone:** [M11 – Special-stage splicing](README.md)  
**Status:** Draft  
**Depends on:** M11-06, M11-07  
**Spec refs:** [15-subsystem-splicing](../../docs/specs/15-subsystem-splicing.md#4-splice-plan-target-sonic-2)

## Story

As a **developer**, I want generated entry/exit shims, V-int routing and a stage selector table, so that the spliced stage starts and finishes like a native one.

## Scope

- Entry shim: target state → donor inputs, VDP setup
- Exit shim: donor results → S2 emerald award/rings, return game mode; calls shared retry (M10-06) and LED (M12) hooks
- V-int routine slot for the spliced stage
- Stage selector table (Q13)

## Acceptance criteria

- [ ] Generated glue assembles; selector routes stage N to the configured stage

## Test plan

- Assemble test; scripted run
