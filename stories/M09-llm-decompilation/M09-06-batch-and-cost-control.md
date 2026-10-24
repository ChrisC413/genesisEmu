# M09-06 · Batch decompilation and cost control

**Milestone:** [M09 – LLM-assisted decompilation](README.md)  
**Status:** Draft  
**Depends on:** M09-05  
**Spec refs:** [11-llm-decompilation](../../docs/specs/11-llm-decompilation.md#workflow)

## Story

As a **reverse engineer**, I want to process a queue of functions (e.g. leaves-first every-frame functions) with cost caps, so that large parts of the ROM get documented without babysitting.

## Scope

- Queue builders (unnamed, hot, every-frame, bottom-up call order)
- Concurrency limit, per-run cost cap, pause/resume
- Usage and cost display

## Acceptance criteria

- [ ] Run stops when cost cap reached
- [ ] Results land as pending proposals

## Test plan

- Mock-provider tests
