# M06-05 · CPU register panels

**Milestone:** [M06 – Debugger & freeze execution](README.md)  
**Status:** Draft  
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

- [ ] Editing D0 then stepping uses the new value
- [ ] Changed registers highlighted after a step

## Test plan

- Manual UI test
