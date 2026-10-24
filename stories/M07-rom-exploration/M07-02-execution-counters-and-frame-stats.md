# M07-02 · Execution counters and per-frame statistics

**Milestone:** [M07 – ROM & memory exploration](README.md)  
**Status:** Draft  
**Depends on:** M07-01  
**Spec refs:** [09-coverage-and-rom-map](../../docs/specs/09-coverage-and-rom-map.md#recorded-data-per-rom-byte)

## Story

As a **developer**, I want per-instruction counters and per-frame statistics over an observation window, so that execution tiers can be computed cheaply and exactly.

## Scope

- Saturating total counters, first/last frame
- `execThisFrame` (lazy reset), `maxPerFrame`
- Observation window (default 300 frames, or the active watch session): frames-executed and executions-in-window
- Dirty-list bookkeeping at frame end

## Acceptance criteria

- [ ] Unit tests: synthetic ROM with code run 1×, 3× total, 1×/frame and 4×/frame yields exact stats
- [ ] Bookkeeping cost stays within the spec 01 budget

## Test plan

- Unit + perf tests
