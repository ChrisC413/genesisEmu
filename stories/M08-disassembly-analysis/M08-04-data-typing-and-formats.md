# M08-04 · Data typing and asset formats

**Milestone:** [M08 – Disassembly & analysis](README.md)  
**Status:** Draft  
**Depends on:** M08-01  
**Spec refs:** [10-disassembler-and-analysis](../../docs/specs/10-disassembler-and-analysis.md#data-typing)

## Story

As a **reverse engineer**, I want to mark data regions with types — including compressed formats, palettes, mappings and structs — with previews, so that data becomes meaningful and exportable as assets.

## Scope

- Type catalog from spec 10
- Decompressors for Nemesis/Kosinski/Enigma/Kosinski-M (for previews and size detection)
- Tile/palette/mapping previews in listing
- User-defined structs

## Acceptance criteria

- [ ] Compressed block length auto-detected by decompressing
- [ ] Tile preview of GHZ art matches the VDP viewer

## Test plan

- Unit tests for each decompressor against known vectors
