# M09-03 · Decompile a function

**Milestone:** [M09 – LLM-assisted decompilation](README.md)  
**Status:** Draft  
**Depends on:** M09-02  
**Spec refs:** [11-llm-decompilation](../../docs/specs/11-llm-decompilation.md#output-schema-abridged), [11-llm-decompilation](../../docs/specs/11-llm-decompilation.md#workflow)

## Story

As a **reverse engineer**, I want to send a function and get pseudo-C, summary and name proposals, so that I understand functions faster.

## Scope

- Versioned prompt templates in repo
- Response parsing to result + proposals
- Errors surfaced with retry

## Acceptance criteria

- [ ] A Sonic 1 function returns schema-valid pseudo-C and ≥ 1 proposal
- [ ] Prompt version stored with result

## Test plan

- Mock-provider tests; opt-in live test
