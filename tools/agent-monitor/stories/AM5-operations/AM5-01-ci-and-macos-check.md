# AM5-01 · CI and macOS check

**Milestone:** [AM5 – Operations](README.md)  
**Status:** Draft  
**Depends on:** AM0-02  
**Spec refs:** [00-overview](../../docs/specs/00-overview.md#testing)  
**Was:** genesisEmu T01-12

## Story

As **Chris (running several agents)**, I want the tests in CI and a recorded macOS run, so that protocol changes can't silently break the monitor.

## Scope

- Workflow step (genesisEmu's after M00-02, or the standalone repo's)
- Hand check on macOS 13+ recorded

## Acceptance criteria

- [ ] CI fails when a test fails
- [ ] Under 30 s added

## Test plan

- CI run on a PR

## Notes

—

<!-- cspell:words ANSI ChrisC413 OQ Ollama PostToolUse Pro Subagent SubagentStop argparse backlogs cmake ctypes dedupe deduplicated fromisoformat genesisEmu gh gitignored isatty jsonl kernel32 lockfile ms msvcrt npm osascript piggy pipx rate_limits relayed relayer relayers statusLine statusline stdlib subagent subagents subtree superproject termios toolu uncached unittest unterminated wintypes worktree worktrees -->
