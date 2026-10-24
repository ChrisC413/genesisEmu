# M11-05 · Shared-dependency equivalence mapping

**Milestone:** [M11 – Special-stage splicing](README.md)  
**Status:** Draft  
**Depends on:** M11-04  
**Spec refs:** [15-subsystem-splicing](../../docs/specs/15-subsystem-splicing.md#4-splice-plan-target-sonic-2)

## Story

As a **reverse engineer**, I want each shared donor routine mapped to a target equivalent (with contract compatibility check) or marked for copying, so that spliced code reuses target services where safe.

## Scope

- Candidate matching (by role, name, code similarity)
- Register-contract compatibility check
- Map vs copy decision table

## Acceptance criteria

- [ ] S1 Nemesis decompressor maps to S2's; incompatible routines default to copy

## Test plan

- Unit tests
