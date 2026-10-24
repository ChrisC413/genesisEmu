# M08-11 · Accuracy scoring against reference disassemblies

**Milestone:** [M08 – Disassembly & analysis](README.md)  
**Status:** Draft  
**Depends on:** M08-07, M08-05  
**Spec refs:** [10-disassembler-and-analysis](../../docs/specs/10-disassembler-and-analysis.md#scoring-against-the-reference)

## Story

As a **developer**, I want scores comparing our code/data, function and jump-table boundaries with the reference, so that analysis quality is measured, not guessed.

## Scope

- Byte-level code/data agreement
- Function and jump-table agreement
- Dashboard panel and CLI report; history over time

## Acceptance criteria

- [ ] Report generated for each game
- [ ] Sonic 1 code/data agreement ≥ 99%

## Test plan

- `[rom]` CTest with thresholds
