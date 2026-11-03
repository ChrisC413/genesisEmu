# M06-01 · Run control: pause, resume, step frame/scanline, reset

**Milestone:** [M06 – Debugger & freeze execution](README.md)  
**Status:** Done  
**Depends on:** —  
**Spec refs:** [08-debugger](../../docs/specs/08-debugger.md#run-control)

## Story

As a **reverse engineer**, I want to freeze execution at an instruction boundary and resume or advance by frame/scanline, so that I can stop the game exactly where something interesting happens.

## Scope

- Run/Pause/Step frame/Step scanline/Reset (soft/hard) commands and hotkeys
- Focused-CPU selector
- Status bar: state, frame, line, MCLK

## Acceptance criteria

- [x] Pause always stops on an instruction boundary: the app pauses between `run()` calls or at a debugger
  stop, which is either before an instruction (exec breakpoint, step) or after a complete one (watchpoint);
  unit tests `[debug][run]` (frame / line limits, soft reset), `app.smoke_pause_at` (`--pause-at`).
- [x] Step frame advances exactly one frame and stops at the first instruction of the next one (unit test
  "Run control"); single-stepping a whole frame ends in the same state as running it ("Stopping ... is exact").
- [ ] Audio silent while paused: the audio stream is paused whenever the emulation is (as before), and
  stepped instructions / lines / frames drop their samples. **Needs a human** to confirm by ear.

## Test plan

- Unit tests on `Machine::run` stop reasons
- Manual UI test

## Notes

- Implementation: `Machine::run(RunLimit::lines(n))`, `Machine::softReset()`, `Debugger::run`, the Debugger
  panel, Emulation menu, status bar and hotkeys (F5, F6, Shift+F6); focus selector for 68000 / Z80.
  `genesisEmu --debugger --pause-at=<frame> --break=<hex>` for scripted checks.
- The core changes keep emulation results identical: every golden (`ctest -L rom`) is unchanged.
