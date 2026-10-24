# AM3 · GitHub Issues sync (optional)

**Goal:** Mirror stories to GitHub Issues one way, safely (priority 3).

**Depends on:** AM2-01

## Testable stage (exit criteria)

`monitor.py issues-plan` for genesisEmu prints a reviewed plan; on a scratch private repository, `issues-sync --apply` creates milestones and issues, a second run reports everything `ok`, an edit on GitHub is reported as a conflict, and a story set to Done closes its issue.

## Stories

| ID | Story | Depends on | Status |
|----|-------|------------|--------|
| AM3-01 | [Dry-run sync planner](AM3-01-dry-run-planner.md) | AM2-01 | Done |
| AM3-02 | [Apply mode (push)](AM3-02-apply-push-mode.md) | AM3-01 | Draft |
| AM3-03 | [Issue comments as feedback](AM3-03-issue-comments-feedback.md) | AM3-02 | Draft |
| AM3-04 | [Link-only mode](AM3-04-link-only-mode.md) | AM3-01 | Draft |

<!-- cspell:words ANSI ChrisC413 OQ Ollama PostToolUse Pro Subagent SubagentStop argparse backlogs cmake ctypes dedupe deduplicated fromisoformat genesisEmu gh gitignored isatty jsonl kernel32 lockfile ms msvcrt npm osascript piggy pipx rate_limits relayed relayer relayers statusLine statusline stdlib subagent subagents subtree superproject termios toolu uncached unittest unterminated wintypes worktree worktrees -->
