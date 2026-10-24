# M08-06 · Symbols, comments and structs

**Milestone:** [M08 – Disassembly & analysis](README.md)  
**Status:** Draft  
**Depends on:** M08-01  
**Spec refs:** [10-disassembler-and-analysis](../../docs/specs/10-disassembler-and-analysis.md#symbols)

## Story

As a **reverse engineer**, I want to rename, comment and define structs, with auto, hardware and RAM symbols, so that the disassembly reads like source.

## Scope

- Symbol sources and precedence
- Rename history
- Local labels
- Hardware register symbol set
- Struct field names in operands (`obj.x_pos(a0)` style rendering)

## Acceptance criteria

- [ ] Renaming updates every reference instantly
- [ ] History shows source of each rename

## Test plan

- Unit tests
