# M06-04 · Memory, VDP and interrupt watchpoints

**Milestone:** [M06 – Debugger & freeze execution](README.md)  
**Status:** Draft  
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

- [ ] Write watch on Sonic's ring counter RAM stops at the ring-collect code
- [ ] DMA-to-CRAM break fires on palette loads

## Test plan

- Unit tests
