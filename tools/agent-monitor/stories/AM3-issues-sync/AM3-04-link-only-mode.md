# AM3-04 · Link-only mode

**Milestone:** [AM3 – GitHub Issues sync (optional)](README.md)  
**Status:** Draft  
**Depends on:** AM3-01  
**Spec refs:** [06-github-issues-sync](../../docs/specs/06-github-issues-sync.md#direction-proposal)

## Story

As **Chris (running several agents)**, I want to link stories to hand-written issues without the tool writing them, so that curated repositories stay untouched.

## Scope

- `issues.mode: link`: `Issue: #N` field in story files; checks existence and state

## Acceptance criteria

- [ ] Never writes to GitHub

## Test plan

- Unit tests

## Notes

—

<!-- cspell:words ANSI ChrisC413 OQ Ollama PostToolUse Pro Subagent SubagentStop argparse backlogs cmake ctypes dedupe deduplicated fromisoformat genesisEmu gh gitignored isatty jsonl kernel32 lockfile ms msvcrt npm osascript piggy pipx rate_limits relayed relayer relayers statusLine statusline stdlib subagent subagents subtree superproject termios toolu uncached unittest unterminated wintypes worktree worktrees -->
