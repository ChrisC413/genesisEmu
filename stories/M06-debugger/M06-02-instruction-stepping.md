# M06-02 · Instruction stepping: into, over, out, run to cursor

**Milestone:** [M06 – Debugger & freeze execution](README.md)  
**Status:** Done  
**Depends on:** M06-01, M06-10  
**Spec refs:** [08-debugger](../../docs/specs/08-debugger.md#run-control)

## Story

As a **reverse engineer**, I want step into/over/out and run-to-cursor on the 68k and Z80, so that I can follow code paths precisely.

## Scope

- Step over for JSR/BSR/TRAP and CALL/RST via stack-depth tracking
- Step out via return detection
- Run to cursor (temp breakpoint)
- Other CPU/chips advance consistently

## Acceptance criteria

- [x] Step over a `jsr` lands on the following instruction even with interrupts in between: unit test (the
  called routine waits for the next V-int) and `[rom]` test in Sonic 1 (a real `jsr` mid-level).
- [x] Stepping the Z80 keeps the 68k in sync: unit test (the 68k never runs ahead by more than the catch-up
  window; step over `call` and step out of the callee on the Z80); semantics in D-031, explained by a tooltip on the Z80 focus button,
  the breakpoint CPU selector, the Z80 disassembly gutter and the "last stop" line.

## Test plan

- Scripted unit tests against synthetic ROMs

## Notes

- Step into / over / out and run to cursor for both CPUs (`Debugger::stepInto`, `beginStepOver`, ...), F11 /
  F10 / Shift+F11 and the Debugger panel; run to cursor from the disassembly context menu.
- A breakpoint or watchpoint that hits first ends a pending step over / out / run-to.
