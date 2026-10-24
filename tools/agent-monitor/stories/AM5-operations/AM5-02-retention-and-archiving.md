# AM5-02 · Retention, filtering and archiving

**Milestone:** [AM5 – Operations](README.md)  
**Status:** Draft  
**Depends on:** AM0-02  
**Spec refs:** [02-monitor-and-views](../../docs/specs/02-monitor-and-views.md)  
**Was:** genesisEmu T01-08

## Story

As **Chris (running several agents)**, I want old agents hidden and finished logs archived, so that the view stays focused (AD-012).

## Scope

- `--since 7d` default
- `archive` moves files of agents whose worktree is gone
- Start near the end of very large files

## Acceptance criteria

- [ ] Never archives a file written in the last hour

## Test plan

- Unit tests

## Notes

—

<!-- cspell:words ANSI ChrisC413 OQ Ollama PostToolUse Pro Subagent SubagentStop argparse backlogs cmake ctypes dedupe deduplicated fromisoformat genesisEmu gh gitignored isatty jsonl kernel32 lockfile ms msvcrt npm osascript piggy pipx rate_limits relayed relayer relayers statusLine statusline stdlib subagent subagents subtree superproject termios toolu uncached unittest unterminated wintypes worktree worktrees -->
