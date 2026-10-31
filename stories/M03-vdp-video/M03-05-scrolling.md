# M03-05 · Horizontal and vertical scrolling

**Milestone:** [M03 – VDP video](README.md)  
**Status:** In progress (S2 EHZ ripple not verified)  
**Depends on:** M03-04  
**Spec refs:** [05-vdp](../../docs/specs/05-vdp.md#rendering)

## Story

As a **developer**, I want all H-scroll (full/cell/line) and V-scroll (full/2-cell) modes, so that parallax and level scrolling are right.

## Scope

- H-scroll table fetch per line
- VSRAM per-column scroll
- Leftmost-column V-scroll quirk (approximate)

## Acceptance criteria

- [x] Sonic 1 GHZ parallax correct in attract demo — frames 1300 / 2000 / 2800: clouds, mountains, the water band and the foreground scroll at different rates (per-line H-scroll)
- [ ] Sonic 2 EHZ water ripple line scroll correct — **not verified**: the EHZ attract demo is the 2-player one; its background renders plausibly but the ripple was not compared frame by frame

## Test plan

- Golden-frame tests

## Notes

- H-scroll: full, per 8-line cell, per line, and the prohibited mode 1 (first 8 entries repeated). 10-bit
  values. V-scroll: full screen or per 2-cell column (10 bits, 11 in interlace mode 2).
- 2-cell columns follow the fine-scrolled fetch grid; the partly visible column left of the first one uses
  VSRAM entries 38 and 39 ANDed (approximation of the hardware quirk).
- Per-line scroll values are recorded for the inspection API (`Vdp::lineInfo()`).
