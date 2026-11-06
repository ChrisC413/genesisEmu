# M06-11 · Call stack view

**Milestone:** [M06 – Debugger & freeze execution](README.md)  
**Status:** Done  
**Depends on:** M06-02  
**Spec refs:** [08-debugger](../../docs/specs/08-debugger.md#inspectors-panels)

## Story

As a **reverse engineer**, I want a heuristic 68k call stack, so that I know how execution reached the current point.

## Scope

- Shadow call stack maintained from JSR/BSR/RTS events (exact) with fallback stack-walk
- Interrupt frames marked

## Acceptance criteria

- [x] Inside an object routine, stack shows main loop → object processing → routine: `[rom]` test (Sonic 1
  at Sonic's jump code: a frame called through ExecuteObjects' `jsr (a1)`), screenshot of the walk.
- [x] V-int frames labeled: unit test ("V-int (level 6)" frame on the synthetic ROM).

## Test plan

- Unit tests with synthetic ROM

## Notes

- Exact shadow stack while "Track calls" is on (JSR / BSR / exceptions, frames popped when SP rises above
  them, so `addq.l #4,sp` style returns work); otherwise a heuristic stack walk over return addresses that
  follow a JSR / BSR in ROM.
