# M06-02 · Instruction stepping: into, over, out, run to cursor

**Milestone:** [M06 – Debugger & freeze execution](README.md)  
**Status:** Draft  
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

- [ ] Step over a `jsr` lands on the following instruction even with interrupts in between
- [ ] Stepping the Z80 keeps the 68k in sync

## Test plan

- Scripted unit tests against synthetic ROMs
