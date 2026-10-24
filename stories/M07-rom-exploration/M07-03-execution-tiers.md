# M07-03 · Execution tiers: once / more than once / per frame / more than per frame

**Milestone:** [M07 – ROM & memory exploration](README.md)  
**Status:** Draft  
**Depends on:** M07-02  
**Spec refs:** [09-coverage-and-rom-map](../../docs/specs/09-coverage-and-rom-map.md#execution-tiers-d-021)

## Story

As a **reverse engineer**, I want each instruction classified per CPU as executed once, more than once, per frame, or more than once per frame, so that I can tell init code, event code, per-frame logic and per-object/inner-loop code apart at a glance.

## Scope

- Tier rules per spec 09 (≥ 95% of window frames; ≤ 1.5 vs > 1.5 executions per frame) — thresholds configurable
- Fallback to 'more than once' when code leaves the window
- Tier exposed to map, listing, tooltips and LLM context

## Acceptance criteria

- [ ] Sonic 1 boot init = once; level load = more than once; V-int handler = per frame; object routines with several objects on screen = more than per frame
- [ ] Title-screen loop is not per frame during a gameplay window

## Test plan

- `[rom]` scripted test asserting tiers of known routines (addresses from the reference labels)
