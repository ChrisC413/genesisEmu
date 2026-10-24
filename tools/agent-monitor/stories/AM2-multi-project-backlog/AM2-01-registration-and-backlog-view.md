# AM2-01 · Project registration file and backlog view

**Milestone:** [AM2 – Multiple projects and backlogs](README.md)  
**Status:** Done  
**Depends on:** AM0-01  
**Spec refs:** [05-multi-project-backlog](../../docs/specs/05-multi-project-backlog.md)

## Story

As **Chris (running several agents)**, I want each repository to declare itself and to see its backlog with agents attached, so that I know what is being worked on and what is left.

## Scope

- `.agent-project.json` (name, GitHub repo, visibility default private, status dir, story globs, issues mode)
- `monitor.py backlog [--project] [--milestone] [--json]`

## Acceptance criteria

- [x] Repositories without the file work with defaults
- [x] Invalid visibility/mode rejected
- [x] Agents shown on the story of their latest event; 'reported done by' when files lag

## Test plan

- `tests/test_backlog.py`

## Notes

—

<!-- cspell:words ANSI ChrisC413 OQ Ollama PostToolUse Pro Subagent SubagentStop argparse backlogs cmake ctypes dedupe deduplicated fromisoformat genesisEmu gh gitignored isatty jsonl kernel32 lockfile ms msvcrt npm osascript piggy pipx rate_limits relayed relayer relayers statusLine statusline stdlib subagent subagents subtree superproject termios toolu uncached unittest unterminated wintypes worktree worktrees -->
