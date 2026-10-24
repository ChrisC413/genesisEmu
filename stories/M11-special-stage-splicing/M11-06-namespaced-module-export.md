# M11-06 · Namespaced, relocatable module export

**Milestone:** [M11 – Special-stage splicing](README.md)  
**Status:** Draft  
**Depends on:** M11-05  
**Spec refs:** [15-subsystem-splicing](../../docs/specs/15-subsystem-splicing.md#4-splice-plan-target-sonic-2)

## Story

As a **reverse engineer**, I want the donor subsystem exported as a prefixed, relocatable AS module, so that it can be dropped into the target source without collisions.

## Scope

- Symbol prefixing (`S1SS_`)
- Only owned + copied members
- Unresolved raw references block export (relocation-risk report)

## Acceptance criteria

- [ ] Module assembles standalone (with stub externs)
- [ ] Zero unresolved references for Sonic 1 special stage

## Test plan

- Export + assemble test
