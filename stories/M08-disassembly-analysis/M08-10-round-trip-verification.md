# M08-10 · Round-trip verification

**Milestone:** [M08 – Disassembly & analysis](README.md)  
**Status:** Draft  
**Depends on:** M08-09  
**Spec refs:** [10-disassembler-and-analysis](../../docs/specs/10-disassembler-and-analysis.md#reassemblable-export)

## Story

As a **developer**, I want automated export → assemble → byte-compare, so that the export is provably faithful.

## Scope

- `gen_cli export --verify`
- First-difference report with address/symbol/context
- CI on homebrew ROMs; local `[rom]` for Sonic

## Acceptance criteria

- [ ] Sonic 1, 2 and 3K round-trip byte-identical

## Test plan

- `[rom]` CTest
