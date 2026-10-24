# AM1-01 · Question, ack and inbox protocol

**Milestone:** [AM1 – Two-way channel](README.md)  
**Status:** Done  
**Depends on:** AM0-01  
**Spec refs:** [03-two-way-channel](../../docs/specs/03-two-way-channel.md#messages), [01-event-protocol](../../docs/specs/01-event-protocol.md#side-channel-states)

## Story

As **Chris (running several agents)**, I want a file format for agent questions and my answers, so that any agent runtime can take part.

## Scope

- `question` and `ack` side states; `closes`
- `<agent>.inbox.jsonl` with answer/steer/note/stop and relayer `delivered` lines
- Delivery state derived from acks + delivered lines (no cursor files)

## Acceptance criteria

- [x] Question status open -> answered -> delivered, or closed
- [x] A message is delivered exactly once per path
- [x] Malformed inbox lines are counted, not fatal

## Test plan

- `tests/test_channel.py`

## Notes

—

<!-- cspell:words ANSI ChrisC413 OQ Ollama PostToolUse Pro Subagent SubagentStop argparse backlogs cmake ctypes dedupe deduplicated fromisoformat genesisEmu gh gitignored isatty jsonl kernel32 lockfile ms msvcrt npm osascript piggy pipx rate_limits relayed relayer relayers statusLine statusline stdlib subagent subagents subtree superproject termios toolu uncached unittest unterminated wintypes worktree worktrees -->
