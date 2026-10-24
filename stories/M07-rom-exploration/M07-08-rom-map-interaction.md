# M07-08 · ROM map interaction

**Milestone:** [M07 – ROM & memory exploration](README.md)  
**Status:** Draft  
**Depends on:** M07-07, M06-06, M06-10  
**Spec refs:** [09-coverage-and-rom-map](../../docs/specs/09-coverage-and-rom-map.md#interaction)

## Story

As a **reverse engineer**, I want hover details, click-to-navigate, range selection actions and bookmarks on the map, so that the map is a navigation hub for analysis.

## Scope

- Tooltip: address, symbol, categories, counts, frames, reader PC, provenance
- Click → disassembly or hex view
- Drag-select: stats, label, set type, breakpoint, queue for analysis
- Bookmarks/regions with notes

## Acceptance criteria

- [ ] Clicking a hot code pixel opens the disassembly at that instruction
- [ ] Range stats correct

## Test plan

- Manual UI test
