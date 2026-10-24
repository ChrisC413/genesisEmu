# AM4 · Capture and usage

**Goal:** Activity, tokens and plan quota without costing agents anything.

**Depends on:** AM0

## Testable stage (exit criteria)

For Claude Code subagents the live view shows liveness and last tool from transcripts and per-agent token counts, and a header shows the 5-hour and 7-day plan usage with reset times.

## Stories

| ID | Story | Depends on | Status |
|----|-------|------------|--------|
| AM4-01 | [Transcript watcher](AM4-01-transcript-watcher.md) | AM0-02 | Draft |
| AM4-02 | [Plan usage from the status line](AM4-02-plan-usage-statusline.md) | AM0-02 | Draft |
| AM4-03 | [Spike: hook latency and coverage](AM4-03-hook-latency-spike.md) | AM4-01 | Draft |

<!-- cspell:words ANSI ChrisC413 OQ Ollama PostToolUse Pro Subagent SubagentStop argparse backlogs cmake ctypes dedupe deduplicated fromisoformat genesisEmu gh gitignored isatty jsonl kernel32 lockfile ms msvcrt npm osascript piggy pipx rate_limits relayed relayer relayers statusLine statusline stdlib subagent subagents subtree superproject termios toolu uncached unittest unterminated wintypes worktree worktrees -->
