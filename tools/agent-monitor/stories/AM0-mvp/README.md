# AM0 · MVP: watch agents live

**Goal:** One terminal view of every agent writing to the status directory.

**Depends on:** —

## Testable stage (exit criteria)

`python tools/agent-monitor/monitor.py --once` on Windows 11 shows each agent's story, state, age, liveness, branch/PR/CI and a recent-events feed; the unittest suite passes.

## Stories

| ID | Story | Depends on | Status |
|----|-------|------------|--------|
| AM0-01 | [Event protocol v1 and emit helper](AM0-01-event-protocol-and-emit.md) | — | Done |
| AM0-02 | [Live terminal monitor](AM0-02-terminal-monitor.md) | AM0-01 | Done |
| AM0-03 | [Branch, commit, PR and CI per agent](AM0-03-git-pr-ci-enrichment.md) | AM0-02 | Done |
| AM0-04 | [Standalone spec, decision and story set](AM0-04-spec-and-story-set.md) | AM0-01 | Done |

<!-- cspell:words ANSI ChrisC413 OQ Ollama PostToolUse Pro Subagent SubagentStop argparse backlogs cmake ctypes dedupe deduplicated fromisoformat genesisEmu gh gitignored isatty jsonl kernel32 lockfile ms msvcrt npm osascript piggy pipx rate_limits relayed relayer relayers statusLine statusline stdlib subagent subagents subtree superproject termios toolu uncached unittest unterminated wintypes worktree worktrees -->
