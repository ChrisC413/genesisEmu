# M03 · VDP video

**Goal:** A complete scanline VDP: planes, scrolling, window, sprites, priority, shadow/highlight, DMA, interrupts, interlace mode 2 — presented in the app.

**Depends on:** M02

## Testable stage (exit criteria)

In the app, Sonic 1/2/3K show the SEGA logo, title screen and attract-mode demos with correct graphics, scrolling, water palettes and sprites. Golden frame hashes are recorded for these checkpoints.

## Notes

Input is not yet available: rely on attract demos and scripted state. Debug viewers (M06-07) help a lot here and may be pulled forward.

## Stories

| ID | Story | Depends on |
|----|-------|------------|
| M03-01 | [VDP ports, registers and memories](M03-01-vdp-ports-registers-memories.md) | — |
| M03-02 | [VDP DMA: 68k transfer, fill, copy](M03-02-dma.md) | M03-01 |
| M03-03 | [H-int, V-int and HV counter](M03-03-interrupts-and-counters.md) | M03-01 |
| M03-04 | [Plane A/B rendering](M03-04-plane-rendering.md) | M03-01 |
| M03-05 | [Horizontal and vertical scrolling](M03-05-scrolling.md) | M03-04 |
| M03-06 | [Window plane](M03-06-window-plane.md) | M03-04 |
| M03-07 | [Sprites](M03-07-sprites.md) | M03-04 |
| M03-08 | [Layer priority and shadow/highlight](M03-08-priority-and-shadow-highlight.md) | M03-05, M03-07 |
| M03-09 | [Interlace mode 2 (double resolution)](M03-09-interlace-mode-2.md) | M03-07 |
| M03-10 | [Frame presentation in the app](M03-10-frame-presentation.md) | M03-04 |
| M03-11 | [Video regression tests](M03-11-video-regression-tests.md) | M03-08 |
