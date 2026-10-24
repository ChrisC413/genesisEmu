# M02-07 · Boot smoke tests and determinism

**Milestone:** [M02 – System bus & boot](README.md)  
**Status:** Draft  
**Depends on:** M02-02, M02-03, M02-04, M02-05  
**Spec refs:** [13-testing-strategy](../../docs/specs/13-testing-strategy.md#layers)

## Story

As a **developer**, I want automated boot tests for all three games plus a determinism check, so that regressions in CPU/bus/scheduler are caught immediately.

## Scope

- `[rom]` tests: run 600 frames per game; assert no illegal/address-error exceptions; assert PC is in the V-int wait loop at frame end
- State hash (xxHash of all component state)
- Run twice → same hash

## Acceptance criteria

- [ ] All three games pass locally with `GENESIS_ROM_DIR` set
- [ ] Tests skip cleanly without ROMs
- [ ] State hash is identical on Windows and macOS

## Test plan

- CTest `[rom]` label
