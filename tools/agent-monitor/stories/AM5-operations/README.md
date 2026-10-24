# AM5 · Operations

**Goal:** Keep the tool reliable and the view focused.

**Depends on:** AM0

## Testable stage (exit criteria)

Tests run in CI on every PR; agents idle for more than 7 days are hidden and archived files are skipped; `monitor.py --serve` shows the same snapshot read-only on localhost; macOS hand check recorded.

## Stories

| ID | Story | Depends on | Status |
|----|-------|------------|--------|
| AM5-01 | [CI and macOS check](AM5-01-ci-and-macos-check.md) | AM0-02 | Draft |
| AM5-02 | [Retention, filtering and archiving](AM5-02-retention-and-archiving.md) | AM0-02 | Draft |
| AM5-03 | [Read-only local web view](AM5-03-local-web-view.md) | AM0-02 | Draft |

<!-- cspell:words ANSI ChrisC413 OQ Ollama PostToolUse Pro Subagent SubagentStop argparse backlogs cmake ctypes dedupe deduplicated fromisoformat genesisEmu gh gitignored isatty jsonl kernel32 lockfile ms msvcrt npm osascript piggy pipx rate_limits relayed relayer relayers statusLine statusline stdlib subagent subagents subtree superproject termios toolu uncached unittest unterminated wintypes worktree worktrees -->
