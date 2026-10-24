# M04-09 · Playability milestone

**Milestone:** [M04 – Input, Z80, playable](README.md)  
**Status:** Draft  
**Depends on:** M04-02, M04-03, M04-07, M04-08  
**Spec refs:** [00-overview](../../docs/specs/00-overview.md#success-criteria-by-phase)

## Story

As a **player**, I want to play the first act of each target game at full speed, so that the emulator core is proven before building the workbench on top.

## Scope

- Performance profiling and fixes
- Bug triage list of visual issues found while playing

## Acceptance criteria

- [ ] GHZ1, EHZ1, AIZ1 completed by a human without visible glitches
- [ ] ≤ 25% of one core used at 60 fps on the reference machine (no instrumentation)
- [ ] Scripted-input regression for each act start

## Test plan

- Manual playtest checklist in the PR
- Benchmark results recorded
