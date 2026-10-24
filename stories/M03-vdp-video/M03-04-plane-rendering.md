# M03-04 · Plane A/B rendering

**Milestone:** [M03 – VDP video](README.md)  
**Status:** Draft  
**Depends on:** M03-01  
**Spec refs:** [05-vdp](../../docs/specs/05-vdp.md#rendering)

## Story

As a **developer**, I want nametable-based plane rendering for H32/H40 with all plane sizes, so that backgrounds and foregrounds appear.

## Scope

- Tile decode (4bpp, flip H/V, palette, priority)
- Plane sizes 32/64/128, mirroring for invalid combos
- Background color, display enable, left-column blank

## Acceptance criteria

- [ ] SEGA logo and Sonic 1 title background render correctly
- [ ] Golden frame hash for Sonic 1 title

## Test plan

- Golden-frame `[rom]` test
