# M06-04 · Memory, VDP and interrupt watchpoints

**Milestone:** [M06 – Debugger & freeze execution](README.md)  
**Status:** Done  
**Depends on:** M06-03  
**Spec refs:** [08-debugger](../../docs/specs/08-debugger.md#breakpoints)

## Story

As a **reverse engineer**, I want to break on memory reads/writes, VDP register writes, DMA, and interrupts/exceptions, so that I can find who touches a variable or hardware register.

## Scope

- Range + size + value conditions
- VDP: register N write, DMA by destination, CRAM/VRAM/VSRAM range writes
- Interrupt level / exception vector breaks
- Break reports the accessing PC

## Acceptance criteria

- [x] Write watch on Sonic's ring counter RAM stops at the ring-collect code: `[rom]` test (the reported PC
  is the `addq.w #1,(v_rings).w` of the ring routine).
- [x] DMA-to-CRAM break fires on palette loads: `[rom]` test (Sonic 1's V-int palette DMA from RAM) and unit
  test (synthetic CRAM DMA).

## Test plan

- Unit tests

## Notes

- Also: VDP register / VRAM / CRAM / VSRAM write ranges, interrupt (68k and Z80) and exception vector breaks,
  frame breaks, Z80 memory watches (the Z80 freezes right after the access). New hooks `onVdpWrite` and
  `onException`.
