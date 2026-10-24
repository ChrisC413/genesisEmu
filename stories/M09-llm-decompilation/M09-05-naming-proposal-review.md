# M09-05 · Naming proposal review

**Milestone:** [M09 – LLM-assisted decompilation](README.md)  
**Status:** Draft  
**Depends on:** M09-03, M08-06  
**Spec refs:** [11-llm-decompilation](../../docs/specs/11-llm-decompilation.md#workflow)

## Story

As a **reverse engineer**, I want to accept, edit or reject each proposed name with evidence and confidence, so that only names I trust enter the symbol DB.

## Scope

- Proposal list with diff, confidence, evidence
- Accept/edit/reject; bulk accept above threshold (opt-in)
- Accepted names source=`llm`, linked to result; undoable

## Acceptance criteria

- [ ] Accepted names show everywhere immediately
- [ ] Rejected proposals don't reappear for the same context hash

## Test plan

- Unit tests; manual UI
