# AM1-07 · Attention signals and notifications

**Milestone:** [AM1 – Two-way channel](README.md)  
**Status:** Draft  
**Depends on:** AM1-03, AM0-03  
**Spec refs:** [00-overview](../../docs/specs/00-overview.md#goals)  
**Was:** genesisEmu T01-05

## Story

As **Chris (running several agents)**, I want to be alerted when an agent asks a blocking question, blocks, fails, goes STALE, breaks CI, or finishes a milestone, so that I don't have to watch the screen (AD-011).

## Scope

- Transition detection (once per transition)
- Terminal bell; `--notify` for OS notifications (Windows toast via PowerShell, macOS `osascript`)
- Attention summary line at the top

## Acceptance criteria

- [ ] Each transition notifies exactly once
- [ ] Off unless `--notify`; bell can be muted with `--quiet`

## Test plan

- Unit tests with a fake clock

## Notes

—

<!-- cspell:words ANSI ChrisC413 OQ Ollama PostToolUse Pro Subagent SubagentStop argparse backlogs cmake ctypes dedupe deduplicated fromisoformat genesisEmu gh gitignored isatty jsonl kernel32 lockfile ms msvcrt npm osascript piggy pipx rate_limits relayed relayer relayers statusLine statusline stdlib subagent subagents subtree superproject termios toolu uncached unittest unterminated wintypes worktree worktrees -->
