# M06-09 · Trace from pause, ring buffer and trace on hit

**Milestone:** [M06 – Debugger & freeze execution](README.md)  
**Status:** Draft  
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

- [ ] Pause → begin trace → step 10 → stop yields exactly 10 records
- [ ] Ring dump after a breakpoint shows the path into it
- [ ] Tracing overhead off when not tracing

## Test plan

- Unit tests; manual UI flow
