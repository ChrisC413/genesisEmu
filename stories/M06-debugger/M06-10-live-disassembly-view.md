# M06-10 · Live disassembly view

**Milestone:** [M06 – Debugger & freeze execution](README.md)  
**Status:** Done  
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

- [x] Stepping keeps the current line highlighted: with Follow PC the listing re-anchors whenever the PC
  leaves the visible lines; the PC row is highlighted (checked in screenshots at a breakpoint in Sonic 1).
  A human may want to hold F11 for a while to confirm it feels right.
- [x] Scrolling backwards doesn't show misaligned instructions for executed code: `previousStart()` prefers
  known instruction starts (the PC, earlier stops, call-stack addresses, the ring buffer) and otherwise the
  candidate with the longest chain of valid instructions; unit test (an immediate that also decodes is
  skipped) and `[rom]` test (8 instructions above Sonic 1's PC line up with the PC).

## Test plan

- Manual UI test

## Notes

- Gutter click toggles an exec breakpoint; right-click: run to cursor, toggle breakpoint, set PC.
- Symbols from the project DB: not yet (the symbol DB arrives with M08). Coverage-based resync arrives with
  M07; until then the known starts are the ones listed above.
