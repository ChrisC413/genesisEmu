# M08-09 · Reassemblable AS export

**Milestone:** [M08 – Disassembly & analysis](README.md)  
**Status:** Draft  
**Depends on:** M08-05, M08-04, M08-06  
**Spec refs:** [10-disassembler-and-analysis](../../docs/specs/10-disassembler-and-analysis.md#reassemblable-export)

## Story

As a **reverse engineer**, I want the project exported as an AS source tree with build scripts, so that I can rebuild and modify the game.

## Scope

- Explicit sizes/branch sizes, encoding-preserving output, `dc.w` fallbacks
- Pointers as label references; relocation-risk report
- Region-grouped files, `binclude` assets
- Z80 driver as separate AS Z80 source
- Build scripts (Windows .bat/.ps1, macOS sh)

## Acceptance criteria

- [ ] Export completes for all three games
- [ ] Relocation-risk report lists every unresolved pointer

## Test plan

- Covered by M08-10
