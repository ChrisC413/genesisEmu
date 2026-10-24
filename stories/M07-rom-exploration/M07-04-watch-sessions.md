# M07-04 · Watch sessions and layers

**Milestone:** [M07 – ROM & memory exploration](README.md)  
**Status:** Draft  
**Depends on:** M07-02, M07-03  
**Spec refs:** [09-coverage-and-rom-map](../../docs/specs/09-coverage-and-rom-map.md#watch-sessions)

## Story

As a **reverse engineer**, I want to arm a watch, trigger an in-game event, stop, and see the code that ran because of it, so that I can find the code behind any event.

## Scope

- Arm/stop UI + hotkey, naming, auto-stop after N frames
- Baseline snapshot; WATCH_NEW; once-after-event
- Saved as layers; layer diff (A − B)
- Breakpoint action: arm watch

## Acceptance criteria

- [ ] Ring-collect watch in Sonic 1 highlights the ring touch routine and SFX trigger, not the main loop
- [ ] Two layers can be diffed

## Test plan

- `[rom]` scripted test: watch around a scripted ring pickup yields a stable address set
