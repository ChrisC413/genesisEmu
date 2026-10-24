# AM1-03 · User CLI and open-questions panel

**Milestone:** [AM1 – Two-way channel](README.md)  
**Status:** Done  
**Depends on:** AM1-01  
**Spec refs:** [03-two-way-channel](../../docs/specs/03-two-way-channel.md#user-interface)

## Story

As **Chris (running several agents)**, I want to see open questions and answer or steer agents from the command line, so that I can unblock them quickly.

## Scope

- `monitor.py answer|steer|note|stop|questions|inbox|pending|mark-delivered`
- OPEN QUESTIONS panel and `ASK` liveness in the live view; JSON snapshot fields

## Acceptance criteria

- [x] `answer` refuses unknown agents/questions unless `--force`
- [x] Blocking unanswered questions show as `ASK`
- [x] Answered-but-undelivered questions stay visible as 'awaiting pickup'

## Test plan

- `tests/test_channel.py`

## Notes

—

<!-- cspell:words ANSI ChrisC413 OQ Ollama PostToolUse Pro Subagent SubagentStop argparse backlogs cmake ctypes dedupe deduplicated fromisoformat genesisEmu gh gitignored isatty jsonl kernel32 lockfile ms msvcrt npm osascript piggy pipx rate_limits relayed relayer relayers statusLine statusline stdlib subagent subagents subtree superproject termios toolu uncached unittest unterminated wintypes worktree worktrees -->
