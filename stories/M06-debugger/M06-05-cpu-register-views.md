# M06-05 · CPU register panels

**Milestone:** [M06 – Debugger & freeze execution](README.md)  
**Status:** Done  
**Depends on:** M06-01  
**Spec refs:** [08-debugger](../../docs/specs/08-debugger.md#inspectors-panels)

## Story

As a **reverse engineer**, I want 68k and Z80 register panels, editable while paused, so that I can see and change CPU state.

## Scope

- Panels per spec 08
- Changed-since-last-stop highlighting
- SR/flags checkboxes
- Edits applied only when paused

## Acceptance criteria

- [x] Editing D0 then stepping uses the new value: the panel writes the CPU state directly (only while
  paused); unit test "A register edited while paused is used by the next step" (also PC edits, which refill
  the prefetch queue).
- [x] Changed registers highlighted after a step: values that differ from the previous stop are drawn in
  amber (checked in screenshots); flags are checkboxes, the IPL mask a slider.

## Test plan

- Manual UI test

## Notes

- Z80 panel: main and shadow sets, IX/IY, SP, PC, I, R, flags, IFF1/2, IM, halted, BUSREQ / RESET, bank,
  INT line. Editing is disabled while running.
