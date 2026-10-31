# M03-09 · Interlace mode 2 (double resolution)

**Milestone:** [M03 – VDP video](README.md)  
**Status:** Done  
**Depends on:** M03-07  
**Spec refs:** [05-vdp](../../docs/specs/05-vdp.md#display-modes)

## Story

As a **developer**, I want interlace mode 2 rendering with 8×16 tiles and 448-line output, so that Sonic 2's 2-player split-screen works.

## Scope

- Tile addressing for 8×16 cells
- Field alternation and odd-frame status bit
- Output 320×448 framebuffer (or weaving option)

## Acceptance criteria

- [x] Sonic 2 2P mode (reached via save state or scripted input later) renders both halves correctly — the Sonic 2 attract sequence includes a 2-player EHZ demo: both halves (title cards, HUDs, Sonic and Tails) render in a 320x448 frame (golden checkpoints at frames 1400 and 2000); the menu path still needs M04 input

## Test plan

- Golden frame once input exists (M04)

## Notes

- Interlace mode 2: 8x16 cells (64-byte tiles), doubled Y for planes and sprites (sprite Y offset 256,
  10-bit Y), 11-bit V-scroll. Each frame renders one field into rows 2*line + field of a 448-line
  framebuffer; the other field's rows stay from the previous frame (weave). The odd-frame status bit
  toggles every frame while interlace is on. Interlace mode 1 renders progressively.
- Unit test: `Interlace mode 2` in `tests/unit/vdp_render_test.cpp`.
