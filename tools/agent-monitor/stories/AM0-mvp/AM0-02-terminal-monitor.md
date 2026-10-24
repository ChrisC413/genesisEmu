# AM0-02 · Live terminal monitor

**Milestone:** [AM0 – MVP: watch agents live](README.md)  
**Status:** Done  
**Depends on:** AM0-01  
**Spec refs:** [02-monitor-and-views](../../docs/specs/02-monitor-and-views.md#views)  
**Was:** genesisEmu T01-02

## Story

As **Chris (running several agents)**, I want a live terminal view of all agents with a recent-events feed, so that I can see what each agent is doing without interrupting it.

## Scope

- Polling tailer with partial-line buffering and truncation handling
- Agent table, liveness, feed, malformed-line summary
- Live mode, `--once`, `--json`; ASCII output; Windows VT mode; refuses non-console output in live mode

## Acceptance criteria

- [x] Runs on Windows 11 with only the Python 3.9+ standard library
- [x] A line being written is never shown half-parsed
- [x] QUIET 5 min / STALE 15 min; done/failed never stale; heartbeats keep an agent live
- [x] `--once` works when redirected

## Test plan

- `tests/test_tail.py`, `tests/test_state.py`, `tests/test_emit_render_git.py::RenderTests`

## Notes

Done for Windows. Still open: a hand check on macOS 13+ (tracked in AM5-01).

<!-- cspell:words ANSI ChrisC413 OQ Ollama PostToolUse Pro Subagent SubagentStop argparse backlogs cmake ctypes dedupe deduplicated fromisoformat genesisEmu gh gitignored isatty jsonl kernel32 lockfile ms msvcrt npm osascript piggy pipx rate_limits relayed relayer relayers statusLine statusline stdlib subagent subagents subtree superproject termios toolu uncached unittest unterminated wintypes worktree worktrees -->
