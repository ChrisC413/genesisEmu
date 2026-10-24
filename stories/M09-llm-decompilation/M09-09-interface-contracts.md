# M09-09 · Function interface contracts with cross-checks

**Milestone:** [M09 – LLM-assisted decompilation](README.md)  
**Status:** Draft  
**Depends on:** M09-03, M09-05  
**Spec refs:** [11-llm-decompilation](../../docs/specs/11-llm-decompilation.md#interface-contracts-for-splicing), [15-subsystem-splicing](../../docs/specs/15-subsystem-splicing.md#3-contract)

## Story

As a **reverse engineer**, I want the LLM to draft an interface contract per function (inputs, outputs, calls, hardware, assumptions) that the tools cross-check against xrefs and coverage, so that I get trustworthy, machine-checkable descriptions of special-stage code to drive splicing.

## Scope

- `contract` request type and schema per spec 11
- Cross-check: claimed RAM inputs/outputs vs observed reads/writes, claimed calls vs xrefs, hardware claims vs observed VDP/IO accesses
- Disagreements flagged; accept/edit per field
- Stored in `contracts` table

## Acceptance criteria

- [ ] A contract for a Sonic 1 special-stage routine is produced; a deliberately wrong claim (injected in a test) is flagged
- [ ] Accepted contracts are queryable by M11 tools

## Test plan

- Mock-provider tests with fixture coverage
- Opt-in live test
