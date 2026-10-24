# M08-05 · Cross-references and function boundaries

**Milestone:** [M08 – Disassembly & analysis](README.md)  
**Status:** Draft  
**Depends on:** M08-02, M08-03  
**Spec refs:** [10-disassembler-and-analysis](../../docs/specs/10-disassembler-and-analysis.md#pipeline)

## Story

As a **reverse engineer**, I want functions with boundaries and complete xrefs (static + observed), so that I can navigate callers/callees and data uses.

## Scope

- Function = entry + reachable blocks until returns/tail jumps
- Xref kinds: call/jump/branch/read/write/pointer; observed flag
- Leaf/every-frame flags from coverage

## Acceptance criteria

- [ ] Xrefs to Sonic's RAM position variable list all writers
- [ ] Function boundaries agree with reference for ≥ 95% of functions

## Test plan

- `[rom]` scoring
