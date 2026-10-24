# M09-08 · Blind evaluation against reference labels

**Milestone:** [M09 – LLM-assisted decompilation](README.md)  
**Status:** Draft  
**Depends on:** M09-06, M08-07  
**Spec refs:** [11-llm-decompilation](../../docs/specs/11-llm-decompilation.md#evaluation)

## Story

As a **developer**, I want a blind evaluation harness scoring proposed names against hidden reference labels, so that we can measure and improve prompt/model choices.

## Scope

- Sample selection (stratified by size/hotness)
- Exact / normalized / LLM-judged semantic match
- Report per prompt version and model

## Acceptance criteria

- [ ] 50-function Sonic 1 report generated
- [ ] Target ≥ 60% combined agreement (success criterion 4)

## Test plan

- Opt-in live run; mock tests for the scoring logic
