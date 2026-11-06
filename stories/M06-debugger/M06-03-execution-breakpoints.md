# M06-03 · Execution breakpoints, conditions and logpoints

**Milestone:** [M06 – Debugger & freeze execution](README.md)  
**Status:** Done  
**Depends on:** M06-01  
**Spec refs:** [08-debugger](../../docs/specs/08-debugger.md#breakpoints)

## Story

As a **reverse engineer**, I want execution breakpoints with conditions, hit counts and logpoints, so that I can stop only when the situation I care about occurs.

## Scope

- Per-address bitmap per CPU
- Expression language → bytecode
- Hit counts, temp, enable/disable
- Logpoints with format strings
- Breakpoints panel

## Acceptance criteria

- [x] Breakpoint on Sonic's jump routine stops when jumping: `[rom]` test (Sonic 1, scripted input; stops at
  frame 1000-1003 where the script presses C).
- [x] Condition `d0.w == $10` evaluated correctly: unit tests (expression parser / evaluator, a breakpoint
  with true and false conditions) and the `[rom]` test (`d0.b == $20` at the jump).
- [x] Performance budget (spec 01) met with 100 breakpoints active: `[rom][perf]` test, Sonic 1: 1.15 ms per
  frame without and 1.82 ms with 100 exec breakpoints (budget: < 8 ms).

## Test plan

- Unit tests for expression parser/evaluator
- Perf benchmark

## Notes

- Breakpoints panel: add / edit / enable / remove, kinds for every breakpoint type, condition, logpoint
  message (and "also stop"), skip-N-hits, temporary, trace-N-frames on hit. Logpoint output is in the
  Debugger panel.
