# AM2 · Multiple projects and backlogs

**Goal:** One monitor for several repositories, each with its backlog and the agents on its stories (priority 2).

**Depends on:** AM0

## Testable stage (exit criteria)

With genesisEmu and agent-monitor registered, `monitor.py --all` shows agents of both with a PROJECT column, and `monitor.py backlog --all` lists each project's milestones with status counts, open stories and which agent is on which story.

## Stories

| ID | Story | Depends on | Status |
|----|-------|------------|--------|
| AM2-01 | [Project registration file and backlog view](AM2-01-registration-and-backlog-view.md) | AM0-01 | Done |
| AM2-02 | [Project registry and cross-project live view](AM2-02-registry-and-cross-project-view.md) | AM2-01 | Draft |
| AM2-03 | [Current work summary per agent](AM2-03-current-work-summary.md) | AM2-01 | Draft |
| AM2-04 | [Move to a standalone private repository](AM2-04-standalone-repository.md) | AM0-04 | Draft |
| AM2-05 | [Register genesisEmu](AM2-05-genesisemu-registration.md) | AM2-01 | Done |

<!-- cspell:words ANSI ChrisC413 OQ Ollama PostToolUse Pro Subagent SubagentStop argparse backlogs cmake ctypes dedupe deduplicated fromisoformat genesisEmu gh gitignored isatty jsonl kernel32 lockfile ms msvcrt npm osascript piggy pipx rate_limits relayed relayer relayers statusLine statusline stdlib subagent subagents subtree superproject termios toolu uncached unittest unterminated wintypes worktree worktrees -->
