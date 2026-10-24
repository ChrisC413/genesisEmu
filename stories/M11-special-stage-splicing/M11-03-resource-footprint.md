# M11-03 · Resource footprint measurement

**Milestone:** [M11 – Special-stage splicing](README.md)  
**Status:** Draft  
**Depends on:** M11-02  
**Spec refs:** [15-subsystem-splicing](../../docs/specs/15-subsystem-splicing.md#2-measure-the-footprint)

## Story

As a **reverse engineer**, I want the subsystem's measured RAM (scratch/inputs/outputs), VRAM/CRAM/VSRAM, VDP settings, interrupts and sound IDs, so that I know what a stage needs from the host game.

## Scope

- Instrumented re-run with scripted input
- RAM split into scratch/inputs/outputs
- VDP register snapshot and ranges
- Sound ID requests

## Acceptance criteria

- [ ] Sonic 1 special-stage footprint report generated; emerald/ring variables appear as inputs/outputs

## Test plan

- `[rom]` test
