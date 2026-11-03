# M06-07 · VDP viewers: registers, tiles, planes, sprites, palette

**Milestone:** [M06 – Debugger & freeze execution](README.md)  
**Status:** Done  
**Depends on:** M06-01  
**Spec refs:** [08-debugger](../../docs/specs/08-debugger.md#inspectors-panels), [05-vdp](../../docs/specs/05-vdp.md#inspection-api)

## Story

As a **reverse engineer**, I want visual VDP inspectors, so that I can see how each frame is built.

## Scope

- Decoded registers/status
- Tile sheet with palette select
- Plane A/B/window images with viewport overlay
- Sprite list + on-screen boxes
- Palette swatches + per-line CRAM changes
- Layer toggles in the game view

## Acceptance criteria

- [ ] Hovering a tile shows its VRAM address and index: implemented (tooltip with tile index, VRAM address
  and a zoomed copy); **needs a human** (hovering cannot be scripted in the screenshot harness).
- [x] Sprite list matches what's on screen in Sonic 1 GHZ: with `--sprite-boxes` the decoded sprites outline
  Sonic, the badnik, rings, bridge logs and the HUD in a GHZ screenshot; `[rom]` test checks the decoded list.

## Test plan

- Manual UI test

## Notes

- Panels: VDP Registers (decoded, status, access / DMA state, layer toggles), Tiles (palette selector),
  Planes (A / B / window with the scroll viewport), Sprites (list and game-view boxes), Palette (swatches and
  CRAM writes per line). `Vdp::decodeSprites()` was declared but never implemented; it is now.
