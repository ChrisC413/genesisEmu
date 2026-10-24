# M03-07 · Sprites

**Milestone:** [M03 – VDP video](README.md)  
**Status:** Draft  
**Depends on:** M03-04  
**Spec refs:** [05-vdp](../../docs/specs/05-vdp.md#rendering)

## Story

As a **developer**, I want sprite rendering with link list, per-line limits, masking and status flags, so that Sonic, rings, enemies and HUD appear correctly.

## Scope

- SAT parse (cached sprite table), sizes 1×1–4×4 cells
- Per-line sprite/pixel limits by mode
- Masking at x=0 rule
- Overflow and collision status bits

## Acceptance criteria

- [ ] Attract demo sprites correct in all three games
- [ ] Sprite overflow behaves on a test ROM

## Test plan

- Golden frames; test ROM
