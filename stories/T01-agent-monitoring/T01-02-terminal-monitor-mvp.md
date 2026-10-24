# T01-02 · Live terminal monitor MVP

**Milestone:** [T01 – Agent monitoring](README.md)  
**Status:** Done  
**Depends on:** T01-01  
**Spec refs:** [17-agent-monitoring](../../docs/specs/17-agent-monitoring.md#architecture) (Architecture), [17-agent-monitoring](../../docs/specs/17-agent-monitoring.md#views) (Views)

## Story

As a **developer**, I want a live terminal view of all agents with a recent-events feed, so that I can see what each agent is doing without interrupting it.

## Scope

- Polling tail of `.agent-status/*.jsonl`, picking up new files; partial last lines buffered; truncation handled
- One row per agent: story, state, last message, age, QUIET/STALE liveness; finished stories
- Recent-events feed across agents; malformed-line summary
- Live full-screen mode, `--once` text snapshot, `--json` snapshot; ASCII output; Windows VT mode

## Acceptance criteria

- [x] Runs on Windows 11 with only the Python 3.9+ standard library
- [ ] Same, checked by hand on macOS 13+ (`monitor.py --once` and live mode in Terminal.app)
- [x] A line being written is never shown half-parsed; an unterminated complete line appears after 3 s, once
- [x] QUIET after 5 min, STALE after 15 min without events; `done`/`failed` agents never go stale; heartbeats keep an agent live
- [x] `--once` works when output is redirected
- [x] Malformed lines never stop the monitor

## Test plan

- `tests/test_tail.py`, `tests/test_state.py`, `tests/test_emit_render_git.py::RenderTests`
- Manual: `monitor.py --once` against the live `.agent-status` while M00 and this workstream report

## Notes

Marked Done for the Windows MVP; the macOS hand check is still open (no Mac in the session that built it). Nothing platform-specific is used on macOS beyond ANSI escapes and `O_APPEND`.
