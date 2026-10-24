# M03-04 · Plane A/B rendering

**Milestone:** [M03 – VDP video](README.md)  
**Status:** Done  
**Depends on:** M03-01  
**Spec refs:** [05-vdp](../../docs/specs/05-vdp.md#rendering)

## Story

As a **developer**, I want nametable-based plane rendering for H32/H40 with all plane sizes, so that backgrounds and foregrounds appear.

## Scope

- Tile decode (4bpp, flip H/V, palette, priority)
- Plane sizes 32/64/128, mirroring for invalid combos
- Background color, display enable, left-column blank

## Acceptance criteria

- [x] SEGA logo and Sonic 1 title background render correctly — checked by eye on dumped frames (SEGA logo frame 150, title frame 600: sky, clouds, mountains, emblem, banner, water)
- [x] Golden frame hash for Sonic 1 title — `tests/golden/m03_frames.json`, Sonic 1 frame 600

## Test plan

- Golden-frame `[rom]` test

## Notes

- `src/core/vdp/vdp_render.cpp`. Each line is rendered at the end of its active period: plane B, plane A
  (or the window), sprites into "pixel code" line buffers, then composed and looked up in a 64-entry
  RGBA cache rebuilt whenever CRAM changed, so palette changes land on the line being drawn.
- Framebuffer: RGBA8888 (bytes R, G, B, A), row stride 320, 256 or 320 wide, 224/240 lines (448/480 in
  interlace mode 2). `Vdp::frameHash()` hashes the visible part.
- Colors use a 15-step DAC ramp (normal = even steps, shadow 0-7, highlight 7-14): 0, 29, 52, 70, 87,
  101, 116, 130, 144, 158, 172, 187, 206, 228, 255 (approximate measured values, not exact).
- Plane size code 2 (prohibited) is treated as 32 cells; name-table addresses wrap inside 8 KB, which
  mirrors invalid size combinations.
- Display disabled: the line shows the backdrop color. Left-8-pixel blanking shows the backdrop.
- Unit tests: `tests/unit/vdp_render_test.cpp`. `gen_cli run --dump-frames N,M --out dir` writes PNGs.
