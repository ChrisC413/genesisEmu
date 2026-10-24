# M11-04 · Subsystem contract

**Milestone:** [M11 – Special-stage splicing](README.md)  
**Status:** Draft  
**Depends on:** M11-03, M09-09  
**Spec refs:** [15-subsystem-splicing](../../docs/specs/15-subsystem-splicing.md#3-contract)

## Story

As a **reverse engineer**, I want a subsystem-level contract assembled from function contracts and the footprint, with LLM-written summary and flags on disagreements, so that the splice has a verified specification.

## Scope

- Aggregate M09-09 contracts + footprint
- Entry/exit/assumptions sections
- Flags block splice builds until resolved

## Acceptance criteria

- [ ] Contract for the Sonic 1 special stage with zero open flags after review

## Test plan

- Fixture tests
