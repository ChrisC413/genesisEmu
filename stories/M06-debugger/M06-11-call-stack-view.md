# M06-11 · Call stack view

**Milestone:** [M06 – Debugger & freeze execution](README.md)  
**Status:** Draft  
**Depends on:** M06-02  
**Spec refs:** [08-debugger](../../docs/specs/08-debugger.md#inspectors-panels)

## Story

As a **reverse engineer**, I want a heuristic 68k call stack, so that I know how execution reached the current point.

## Scope

- Shadow call stack maintained from JSR/BSR/RTS events (exact) with fallback stack-walk
- Interrupt frames marked

## Acceptance criteria

- [ ] Inside an object routine, stack shows main loop → object processing → routine
- [ ] V-int frames labeled

## Test plan

- Unit tests with synthetic ROM
