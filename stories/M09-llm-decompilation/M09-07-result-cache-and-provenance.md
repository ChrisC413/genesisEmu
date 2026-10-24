# M09-07 · Result cache and provenance

**Milestone:** [M09 – LLM-assisted decompilation](README.md)  
**Status:** Draft  
**Depends on:** M09-03  
**Spec refs:** [11-llm-decompilation](../../docs/specs/11-llm-decompilation.md#caching-and-provenance)

## Story

As a **developer**, I want LLM results cached by content hashes and recorded with model, prompt version and usage, so that repeat requests are free and every name is traceable.

## Scope

- Cache key per spec 11
- Provenance fields stored
- Invalidate when bundle changes

## Acceptance criteria

- [ ] Identical request served from cache (no network)
- [ ] Changing a callee name changes the context hash

## Test plan

- Unit tests
