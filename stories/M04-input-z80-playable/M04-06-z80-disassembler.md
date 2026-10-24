# M04-06 · Z80 disassembler (AS syntax)

**Milestone:** [M04 – Input, Z80, playable](README.md)  
**Status:** Draft  
**Depends on:** M04-04  
**Spec refs:** [03-cpu-z80](../../docs/specs/03-cpu-z80.md#disassembler)

## Story

As a **reverse engineer**, I want a Z80 disassembler sharing the 68k disassembler's output structure, so that sound-driver code is readable in the debugger and analysis tools.

## Scope

- All documented + undocumented opcodes
- Flow info and targets

## Acceptance criteria

- [ ] Round-trip through AS for all opcodes

## Test plan

- Corpus round-trip test
