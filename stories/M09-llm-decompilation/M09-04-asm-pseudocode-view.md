# M09-04 · Side-by-side asm ↔ pseudo-C view

**Milestone:** [M09 – LLM-assisted decompilation](README.md)  
**Status:** Draft  
**Depends on:** M09-03, M08-08  
**Spec refs:** [11-llm-decompilation](../../docs/specs/11-llm-decompilation.md#workflow)

## Story

As a **reverse engineer**, I want pseudo-C shown next to the asm with linked line highlighting, so that I can verify the LLM's reading against the real code.

## Scope

- `line_map` driven highlighting both directions
- Syntax highlighting; symbols clickable
- Version history of results per function

## Acceptance criteria

- [ ] Clicking a pseudo-C line highlights its asm addresses and vice-versa

## Test plan

- Manual UI test
