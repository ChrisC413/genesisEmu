# M11-10 · Splice equivalence harness

**Milestone:** [M11 – Special-stage splicing](README.md)  
**Status:** Draft  
**Depends on:** M11-08  
**Spec refs:** [15-subsystem-splicing](../../docs/specs/15-subsystem-splicing.md#5-validation)

## Story

As a **developer**, I want a harness comparing the stage running in the donor vs spliced into the target, frame by frame, after applying the RAM map, so that I can prove the transplant behaves like the original.

## Scope

- Record owned RAM, CRAM, owned-VRAM hashes, sound IDs per frame
- Equivalent scripted input in both games
- First-divergence report with both traces; allowlist for expected differences

## Acceptance criteria

- [ ] Harness detects an injected divergence
- [ ] Reports are reproducible

## Test plan

- Unit tests; `[rom]` run
