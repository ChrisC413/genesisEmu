# M08-08 · Analysis listing UI

**Milestone:** [M08 – Disassembly & analysis](README.md)  
**Status:** Draft  
**Depends on:** M08-05, M08-06  
**Spec refs:** [10-disassembler-and-analysis](../../docs/specs/10-disassembler-and-analysis.md#listing-view-ui)

## Story

As a **reverse engineer**, I want a navigable, virtualized listing of the whole ROM with keyboard-driven RE workflow, so that I can work through the ROM efficiently.

## Scope

- Columns: address, bytes, label, instruction/data, comment, xrefs
- Go to, back/forward, follow operand, xrefs popup, rename, comment, set type hotkeys
- Sync with ROM map and debugger
- Symbol browser panel, function list with filters (unnamed, hot, every-frame)

## Acceptance criteria

- [ ] Scrolling the full 4 MB S3K listing stays at 60 fps
- [ ] All hotkeys work and are documented

## Test plan

- Manual UI test
