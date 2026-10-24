# M11-11 · First splice: Sonic 1 special stage into Sonic 2

**Milestone:** [M11 – Special-stage splicing](README.md)  
**Status:** Draft  
**Depends on:** M11-09, M11-10  
**Spec refs:** [15-subsystem-splicing](../../docs/specs/15-subsystem-splicing.md#5-validation)

## Story

As a **player**, I want the Sonic 1 rotating special stage inside Sonic 2, so that the splicing pipeline is proven end-to-end.

## Scope

- Full plan → build → validate
- Retry and LED hooks invoked on exit

## Acceptance criteria

- [ ] Stage reachable via the selector; success awards the correct S2 emerald; failure returns (or retries)
- [ ] Equivalence harness: no unexpected divergence over the scripted run
- [ ] Strict mode clean; flash-cart test passed

## Test plan

- `[rom]` end-to-end scripted test
- Hardware checklist
