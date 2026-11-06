# M06-06 · Memory viewers / hex editor

**Milestone:** [M06 – Debugger & freeze execution](README.md)  
**Status:** Done  
**Depends on:** M06-01  
**Spec refs:** [08-debugger](../../docs/specs/08-debugger.md#inspectors-panels)

## Story

As a **reverse engineer**, I want hex views of ROM, 68k RAM, Z80 RAM, VRAM, CRAM, VSRAM and SRAM, so that I can explore and modify any memory.

## Scope

- Virtualized hex grid with ASCII
- Go-to address/symbol, byte-pattern search
- Recent-write highlighting
- Edit while paused (ROM edits marked as patches)
- Multiple viewer instances

## Acceptance criteria

- [x] Searching for a byte pattern finds all occurrences in ROM: unit tests for the parser and search
  (overlapping hits, `??` wildcards) and the `[rom]` tests locate Sonic 1 routines by pattern.
- [x] Edits reflected in emulation after resume: unit test (a ROM patch changes the value a routine stores;
  RAM and CRAM edits).

## Test plan

- Manual UI test; unit tests for search

## Notes

- Four Memory panels (ROM, 68k RAM, Z80 RAM, VRAM, CRAM, VSRAM, SRAM), go-to address, byte search with
  next / previous, click-to-edit while paused, bytes changed since the previous stop in amber, ROM edits
  listed as patches (lost when the ROM is reloaded).
- Go to *symbol* needs the symbol DB (M08).
