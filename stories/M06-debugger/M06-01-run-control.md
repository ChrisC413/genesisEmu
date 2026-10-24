# M06-01 · Run control: pause, resume, step frame/scanline, reset

**Milestone:** [M06 – Debugger & freeze execution](README.md)  
**Status:** Draft  
**Depends on:** —  
**Spec refs:** [08-debugger](../../docs/specs/08-debugger.md#run-control)

## Story

As a **reverse engineer**, I want to freeze execution at an instruction boundary and resume or advance by frame/scanline, so that I can stop the game exactly where something interesting happens.

## Scope

- Run/Pause/Step frame/Step scanline/Reset (soft/hard) commands and hotkeys
- Focused-CPU selector
- Status bar: state, frame, line, MCLK

## Acceptance criteria

- [ ] Pause always stops on an instruction boundary
- [ ] Step frame advances exactly one frame
- [ ] Audio silent while paused

## Test plan

- Unit tests on `Machine::run` stop reasons
- Manual UI test
