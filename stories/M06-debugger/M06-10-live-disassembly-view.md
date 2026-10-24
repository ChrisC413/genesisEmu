# M06-10 · Live disassembly view

**Milestone:** [M06 – Debugger & freeze execution](README.md)  
**Status:** Draft  
**Depends on:** M06-01  
**Spec refs:** [08-debugger](../../docs/specs/08-debugger.md#inspectors-panels)

## Story

As a **reverse engineer**, I want a disassembly panel that follows the PC with breakpoint gutter, so that I always see the code that's executing.

## Scope

- Uses M01-12 / M04-06 disassemblers
- Follow-PC, scroll back heuristically (resync on known instruction starts from coverage)
- Click gutter to toggle breakpoint
- Symbols from project DB when available

## Acceptance criteria

- [ ] Stepping keeps the current line highlighted
- [ ] Scrolling backwards doesn't show misaligned instructions for executed code

## Test plan

- Manual UI test
