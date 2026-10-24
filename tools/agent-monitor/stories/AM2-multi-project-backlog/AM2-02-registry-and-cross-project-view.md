# AM2-02 · Project registry and cross-project live view

**Milestone:** [AM2 – Multiple projects and backlogs](README.md)  
**Status:** Draft  
**Depends on:** AM2-01  
**Spec refs:** [05-multi-project-backlog](../../docs/specs/05-multi-project-backlog.md#registry-per-machine-not-committed)

## Story

As **Chris (running several agents)**, I want one live view across all my projects, so that I can supervise agents in different repositories together.

## Scope

- `monitor.py projects add|list|remove`
- `--all` tails every registered status dir; PROJECT column; rows keyed by (project, agent)
- `answer`/`steer` accept `project:agent`

## Acceptance criteria

- [ ] Same agent name in two projects shows two rows
- [ ] A missing project path is reported, not fatal

## Test plan

- Unit tests with two temporary projects

## Notes

—

<!-- cspell:words ANSI ChrisC413 OQ Ollama PostToolUse Pro Subagent SubagentStop argparse backlogs cmake ctypes dedupe deduplicated fromisoformat genesisEmu gh gitignored isatty jsonl kernel32 lockfile ms msvcrt npm osascript piggy pipx rate_limits relayed relayer relayers statusLine statusline stdlib subagent subagents subtree superproject termios toolu uncached unittest unterminated wintypes worktree worktrees -->
