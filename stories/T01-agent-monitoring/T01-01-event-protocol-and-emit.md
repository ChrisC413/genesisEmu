# T01-01 · Event protocol v1 and emit helper

**Milestone:** [T01 – Agent monitoring](README.md)  
**Status:** Done  
**Depends on:** —  
**Spec refs:** [17-agent-monitoring](../../docs/specs/17-agent-monitoring.md#event-protocol) (Event protocol)

## Story

As a **developer**, I want a documented, append-only event format and a helper that writes it correctly, so that every agent reports the same way and a monitor can trust what it reads.

## Scope

- Formalize the existing line format as v1 (fields, states, location, one writer per file)
- Additive extensions: `heartbeat`, `percent`, `branch`, `worktree`, `pr`, `tests`, `tokens`, `cost_usd`, `session_id`, `host`, `detail`
- `tools/agent-monitor/emit.py` CLI and `emit()` library function: validation, UTC `ts`, one-line message, git context, atomic append

## Acceptance criteria

- [x] v1 lines written before this story parse unchanged
- [x] Unknown fields are preserved, unknown states shown verbatim
- [x] Malformed, partial, empty, non-object and bad-timestamp lines raise a parse error with file and line number
- [x] `emit` output round-trips through the parser
- [x] Four concurrent writers appending 200 lines lose none (Windows and POSIX)

## Test plan

- `tests/test_protocol.py`, `tests/test_emit_render_git.py::EmitTests`

## Notes

Found during testing: the Windows C runtime's `O_APPEND` is seek-then-write; concurrent writers lost 18 of 200 lines. The helper uses a `FILE_APPEND_DATA` handle on Windows.
