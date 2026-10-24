# M08-03 · Jump and pointer table detection

**Milestone:** [M08 – Disassembly & analysis](README.md)  
**Status:** Draft  
**Depends on:** M08-02  
**Spec refs:** [10-disassembler-and-analysis](../../docs/specs/10-disassembler-and-analysis.md#code-discovery)

## Story

As a **reverse engineer**, I want Sonic-style jump tables and pointer tables detected and bounded, so that object routines and state machines are fully discovered.

## Scope

- Idioms from spec 10
- Bounds from observed indices + heuristics
- Runtime-observed indirect targets as xrefs
- Unresolved tables flagged

## Acceptance criteria

- [ ] Sonic 1 object routine tables resolved with correct counts (spot-check 20 against s1disasm)

## Test plan

- `[rom]` tests
