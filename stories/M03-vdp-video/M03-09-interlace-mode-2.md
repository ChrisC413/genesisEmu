# M03-09 · Interlace mode 2 (double resolution)

**Milestone:** [M03 – VDP video](README.md)  
**Status:** Draft  
**Depends on:** M03-07  
**Spec refs:** [05-vdp](../../docs/specs/05-vdp.md#display-modes)

## Story

As a **developer**, I want interlace mode 2 rendering with 8×16 tiles and 448-line output, so that Sonic 2's 2-player split-screen works.

## Scope

- Tile addressing for 8×16 cells
- Field alternation and odd-frame status bit
- Output 320×448 framebuffer (or weaving option)

## Acceptance criteria

- [ ] Sonic 2 2P mode (reached via save state or scripted input later) renders both halves correctly

## Test plan

- Golden frame once input exists (M04)
