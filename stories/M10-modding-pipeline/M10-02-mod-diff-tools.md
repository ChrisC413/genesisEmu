# M10-02 · Mod diff tools

**Milestone:** [M10 – Modding pipeline & first Sonic 2 mods](README.md)  
**Status:** Draft  
**Depends on:** M10-01  
**Spec refs:** [14-modding-pipeline](../../docs/specs/14-modding-pipeline.md#pipeline)

## Story

As a **reverse engineer**, I want byte, symbol, coverage and frame diffs between original and mod, so that I can confirm a mod changes only what I intended.

## Scope

- Byte + symbol-level diff
- Coverage diff from the same scripted input
- Frame-hash diff at checkpoints
- Relocation-risk warnings before build

## Acceptance criteria

- [ ] A canned trivial mod shows frame diffs only after the changed behavior; coverage diff shows exactly the changed code

## Test plan

- `[rom]` test with a canned mod patch
