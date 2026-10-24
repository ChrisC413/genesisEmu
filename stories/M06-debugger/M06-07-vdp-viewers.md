# M06-07 · VDP viewers: registers, tiles, planes, sprites, palette

**Milestone:** [M06 – Debugger & freeze execution](README.md)  
**Status:** Draft  
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

- [ ] Hovering a tile shows its VRAM address and index
- [ ] Sprite list matches what's on screen in Sonic 1 GHZ

## Test plan

- Manual UI test
