# M06-03 · Execution breakpoints, conditions and logpoints

**Milestone:** [M06 – Debugger & freeze execution](README.md)  
**Status:** Draft  
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

- [ ] Breakpoint on Sonic's jump routine stops when jumping
- [ ] Condition `d0.w == $10` evaluated correctly
- [ ] Performance budget (spec 01) met with 100 breakpoints active

## Test plan

- Unit tests for expression parser/evaluator
- Perf benchmark
