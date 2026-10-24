# AM0-01 · Event protocol v1 and emit helper

**Milestone:** [AM0 – MVP: watch agents live](README.md)  
**Status:** Done  
**Depends on:** —  
**Spec refs:** [01-event-protocol](../../docs/specs/01-event-protocol.md)  
**Was:** genesisEmu T01-01

## Story

As **Chris (running several agents)**, I want a documented append-only event format and a helper that writes it correctly, so that every agent reports the same way.

## Scope

- v1 format frozen; additive extensions
- `emit.py` CLI and `emit()`: validation, UTC `ts`, one-line message, git context, atomic append

## Acceptance criteria

- [x] v1 lines written before this story parse unchanged
- [x] Unknown fields preserved; unknown states shown verbatim
- [x] Malformed/partial/empty lines rejected with file:line
- [x] 4 concurrent writers x 50 lines lose nothing on Windows

## Test plan

- `tests/test_protocol.py`, `tests/test_emit_render_git.py::EmitTests`

## Notes

Found: the Windows CRT `O_APPEND` loses lines under concurrency (182/200); fixed with `FILE_APPEND_DATA`.

<!-- cspell:words ANSI ChrisC413 OQ Ollama PostToolUse Pro Subagent SubagentStop argparse backlogs cmake ctypes dedupe deduplicated fromisoformat genesisEmu gh gitignored isatty jsonl kernel32 lockfile ms msvcrt npm osascript piggy pipx rate_limits relayed relayer relayers statusLine statusline stdlib subagent subagents subtree superproject termios toolu uncached unittest unterminated wintypes worktree worktrees -->
