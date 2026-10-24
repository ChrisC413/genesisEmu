# M09-02 · Context bundle builder

**Milestone:** [M09 – LLM-assisted decompilation](README.md)  
**Status:** Draft  
**Depends on:** M09-01  
**Spec refs:** [11-llm-decompilation](../../docs/specs/11-llm-decompilation.md#context-bundle)

## Story

As a **developer**, I want a builder that assembles the per-function context from the project DB and coverage, so that the LLM gets exactly the information a human RE would want.

## Scope

- Sections per spec 11 (including execution tier and data categories)
- Size budget with prioritized truncation
- Blind mode substitution
- Optional UI preview of the exact payload (not a required gate, D-018)

## Acceptance criteria

- [ ] Bundle for a known function contains callers, callees, RAM usage and runtime facts
- [ ] Blind bundle contains no reference-sourced names

## Test plan

- Unit tests with fixture projects
