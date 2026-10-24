# M03-05 · Horizontal and vertical scrolling

**Milestone:** [M03 – VDP video](README.md)  
**Status:** Draft  
**Depends on:** M03-04  
**Spec refs:** [05-vdp](../../docs/specs/05-vdp.md#rendering)

## Story

As a **developer**, I want all H-scroll (full/cell/line) and V-scroll (full/2-cell) modes, so that parallax and level scrolling are right.

## Scope

- H-scroll table fetch per line
- VSRAM per-column scroll
- Leftmost-column V-scroll quirk (approximate)

## Acceptance criteria

- [ ] Sonic 1 GHZ parallax correct in attract demo
- [ ] Sonic 2 EHZ water ripple line scroll correct

## Test plan

- Golden-frame tests
