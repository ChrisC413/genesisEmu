# AM2-03 · Current work summary per agent

**Milestone:** [AM2 – Multiple projects and backlogs](README.md)  
**Status:** Draft  
**Depends on:** AM2-01  
**Spec refs:** [05-multi-project-backlog](../../docs/specs/05-multi-project-backlog.md#views)  
**Was:** genesisEmu T01-06

## Story

As **Chris (running several agents)**, I want a one-line summary of what each agent is targeting now, so that I understand the work without reading files (MQ9).

## Scope

- Story title from the backlog + the agent's latest message + latest commit subject on its branch

## Acceptance criteria

- [ ] Shown under each row and in `--json`
- [ ] No acceptance-criteria parsing

## Test plan

- Unit test of the summary builder

## Notes

—

<!-- cspell:words ANSI ChrisC413 OQ Ollama PostToolUse Pro Subagent SubagentStop argparse backlogs cmake ctypes dedupe deduplicated fromisoformat genesisEmu gh gitignored isatty jsonl kernel32 lockfile ms msvcrt npm osascript piggy pipx rate_limits relayed relayer relayers statusLine statusline stdlib subagent subagents subtree superproject termios toolu uncached unittest unterminated wintypes worktree worktrees -->
