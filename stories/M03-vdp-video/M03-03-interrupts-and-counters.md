# M03-03 · H-int, V-int and HV counter

**Milestone:** [M03 – VDP video](README.md)  
**Status:** Draft  
**Depends on:** M03-01  
**Spec refs:** [05-vdp](../../docs/specs/05-vdp.md#interrupts-and-timing)

## Story

As a **developer**, I want accurate per-line interrupt timing and HV counter values, so that water effects and raster tricks happen on the correct lines.

## Scope

- H-int counter reload/decrement
- V-int pending flag, acknowledge
- HV counter tables for H32/H40, V28/V30, interlace
- Z80 /INT assertion for one line

## Acceptance criteria

- [ ] Labyrinth Zone water line in Sonic 1 appears at the right height
- [ ] Unit tests for counter sequences

## Test plan

- Unit tests; visual check
