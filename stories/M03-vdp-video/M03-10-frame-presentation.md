# M03-10 · Frame presentation in the app

**Milestone:** [M03 – VDP video](README.md)  
**Status:** In progress (pacing on a 60 Hz display not verified)  
**Depends on:** M03-04  
**Spec refs:** [07-frontend-platform](../../docs/specs/07-frontend-platform.md#frame-pacing), [07-frontend-platform](../../docs/specs/07-frontend-platform.md#application-shell)

## Story

As a **player**, I want the emulated framebuffer shown in the game view with scaling and correct pacing, so that I can watch the game run smoothly.

## Scope

- Framebuffer → GPU texture each frame
- Integer scale / fit, aspect correction, filtering options
- Frame pacing at 59.92 Hz with vsync; fast-forward
- Run/Pause/Reset menu wired to the core

## Acceptance criteria

- [ ] No tearing; steady frame pacing on a 60 Hz display — **not verified**: the reference machine's display runs at about 100 Hz (the smoke test ran at 99.9 ticks/s with vsync), so the vsync-locked path was not exercised; the timer-paced path for other refresh rates is used there
- [x] Resizing window keeps aspect — the picture is letterboxed at 4:3 (or square pixels with correction off) in any panel size; checked on a screenshot from a local run on the real GPU
- [x] Fast-forward runs ≥ 5× on the reference machine — `--fast-forward` smoke run of S3K: peak 1657 emulated frames/s (≈ 27×), core 0.62 ms/frame

## Test plan

- Manual check on both OSes

## Notes

- `App` owns a `Machine` (created when a ROM opens; `--lock-on=<file>` for S&K + Sonic 3). Each UI tick runs
  emulation before the ImGui frame: one emulated frame per vsync when the display refresh is within 1 Hz of
  59.92 / 49.70 Hz, else an accumulator against the real clock (max 4 frames per tick). Fast-forward
  (hold Tab, the Emulation menu, or `--fast-forward`) runs frames for ~12 ms per tick.
- Emulation menu: Run / Pause (F5), Reset, Fast-forward. Game view context menu (right click): fit /
  integer scale, 4:3 aspect correction, linear filtering, frame-time overlay (fps and core ms/frame).
- The framebuffer goes to a 320x480 GL texture with `glTexSubImage2D` every emulated tick; interlace mode 2
  frames are shown at the same picture height as progressive ones.
- Smoke test: with a ROM it now also fails unless frames were emulated and reached the game view, and logs
  the frame hash. Local run on the real GPU (Sonic 1, 1300 frames): core 0.66 ms/frame on average.
- View settings are not persisted yet.
