# AM4-03 · Spike: hook latency and coverage

**Milestone:** [AM4 – Capture and usage](README.md)  
**Status:** Draft  
**Depends on:** AM4-01  
**Spec refs:** [04-capture-and-usage](../../docs/specs/04-capture-and-usage.md#recommendation)

## Story

As **Chris (running several agents)**, I want measured numbers for async and sync hooks, so that hooks are only adopted if they don't slow agents.

## Scope

- Measure B1/B2 on Windows and macOS with a minimal script; verify hooks fire for subagents

## Acceptance criteria

- [ ] Written finding with numbers; recommendation updated in spec 04

## Test plan

- —

## Notes

Deferred: hooks are not to be implemented yet (Chris, MQ7).

<!-- cspell:words ANSI ChrisC413 OQ Ollama PostToolUse Pro Subagent SubagentStop argparse backlogs cmake ctypes dedupe deduplicated fromisoformat genesisEmu gh gitignored isatty jsonl kernel32 lockfile ms msvcrt npm osascript piggy pipx rate_limits relayed relayer relayers statusLine statusline stdlib subagent subagents subtree superproject termios toolu uncached unittest unterminated wintypes worktree worktrees -->
