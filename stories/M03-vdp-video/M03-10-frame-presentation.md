# M03-10 · Frame presentation in the app

**Milestone:** [M03 – VDP video](README.md)  
**Status:** Draft  
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

- [ ] No tearing; steady frame pacing on a 60 Hz display
- [ ] Resizing window keeps aspect
- [ ] Fast-forward runs ≥ 5× on the reference machine

## Test plan

- Manual check on both OSes
