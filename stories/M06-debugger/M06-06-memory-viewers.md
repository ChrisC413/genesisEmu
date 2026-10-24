# M06-06 · Memory viewers / hex editor

**Milestone:** [M06 – Debugger & freeze execution](README.md)  
**Status:** Draft  
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

- [ ] Searching for a byte pattern finds all occurrences in ROM
- [ ] Edits reflected in emulation after resume

## Test plan

- Manual UI test; unit tests for search
