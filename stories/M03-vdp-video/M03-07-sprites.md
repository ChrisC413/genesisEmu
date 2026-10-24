# M03-07 · Sprites

**Milestone:** [M03 – VDP video](README.md)  
**Status:** Done  
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

- [x] Attract demo sprites correct in all three games — Sonic, Tails, Knuckles, rings, badniks, HUD and title-screen sprites look right in the dumped frames (S1 GHZ, S2 EHZ 2P / CPZ / ARZ, S3K AIZ intro and demo)
- [x] Sprite overflow behaves on a test ROM — verified with a synthetic unit test (21 sprites on one H40 line: the 21st is dropped and the overflow flag is set) instead of a test ROM

## Test plan

- Golden frames; test ROM

## Notes

- Link-list walk from sprite 0 (80 sprites in H40, 64 in H32), up to 20 / 16 sprites and 320 / 256
  sprite pixels per line; Y / size / link come from the internal sprite cache (M03-01).
- Masking: a sprite with X = 0 hides the rest of the line once a sprite with X != 0 was seen on that line,
  or when the previous line ran out of sprite pixels.
- Collision: two opaque sprite pixels on the same spot. Overflow: per-line sprite or pixel limit hit.
