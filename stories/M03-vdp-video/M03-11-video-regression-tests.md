# M03-11 · Video regression tests

**Milestone:** [M03 – VDP video](README.md)  
**Status:** Draft  
**Depends on:** M03-08  
**Spec refs:** [13-testing-strategy](../../docs/specs/13-testing-strategy.md#test-data-policy)

## Story

As a **developer**, I want golden frame hashes at fixed frames of each game's boot/attract sequence, so that VDP regressions are caught.

## Scope

- `gen_cli run --check` with checkpoint frame hashes
- Golden file management per spec 13
- Diff image output on mismatch (expected/actual/diff PNG)

## Acceptance criteria

- [ ] ≥ 5 checkpoints per game
- [ ] Mismatch produces diff images

## Test plan

- `[rom]` CTest
