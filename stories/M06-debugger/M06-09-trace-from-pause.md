# M06-09 · Trace from pause, ring buffer and trace on hit

**Milestone:** [M06 – Debugger & freeze execution](README.md)  
**Status:** Done (text output; binary format not done)  
**Depends on:** M06-03  
**Spec refs:** [08-debugger](../../docs/specs/08-debugger.md#tracing-trace-from-pause-d-007)

## Story

As a **reverse engineer**, I want to begin tracing from a paused state, with filters, and to dump the last N instructions after a break, so that I can capture exactly the execution I want to study.

## Scope

- Trace panel: configure CPU/range/first-execution-only/max size; Begin/Stop
- Text + binary output (reuses M02-06 writer)
- Always-on ring buffer (default 100k) dumpable on break
- Breakpoint action: begin trace for N frames

## Acceptance criteria

- [x] Pause → begin trace → step 10 → stop yields exactly 10 records: unit test.
- [x] Ring dump after a breakpoint shows the path into it: unit test (the last 68k entry is the `rts` before
  the breakpoint).
- [x] Tracing overhead off when not tracing: the trace hooks are removed when it stops (unit test checks
  that no hook is installed) and frame time with an idle debugger is unchanged (PR performance table).

## Test plan

- Unit tests; manual UI flow

## Notes

- Trace panel: file, PC range, first execution only, frames, record limit, begin / stop; ring buffer switch
  (opt-in, D-030), size and dump. Breakpoints can begin a trace of N frames on hit.
- The 68k is traced in the M02-06 text format; the ring buffer covers both CPUs. The compact binary format
  is not implemented yet.
