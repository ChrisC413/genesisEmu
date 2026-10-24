# AM6 · Local agents and multiple machines

**Goal:** Non-Claude agents and more than one machine (low priority).

**Depends on:** AM1

## Testable stage (exit criteria)

An agent loop driven by a local Ollama model reports events, asks a question and receives an answer through the same files; a second machine's agents appear in the same view.

## Stories

| ID | Story | Depends on | Status |
|----|-------|------------|--------|
| AM6-01 | [Local agents on Ollama](AM6-01-ollama-local-agents.md) | AM1-02 | Draft |
| AM6-02 | [Multi-machine aggregation](AM6-02-multi-machine.md) | AM2-02 | Draft |

<!-- cspell:words ANSI ChrisC413 OQ Ollama PostToolUse Pro Subagent SubagentStop argparse backlogs cmake ctypes dedupe deduplicated fromisoformat genesisEmu gh gitignored isatty jsonl kernel32 lockfile ms msvcrt npm osascript piggy pipx rate_limits relayed relayer relayers statusLine statusline stdlib subagent subagents subtree superproject termios toolu uncached unittest unterminated wintypes worktree worktrees -->
