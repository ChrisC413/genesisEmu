# AM3-02 · Apply mode (push)

**Milestone:** [AM3 – GitHub Issues sync (optional)](README.md)  
**Status:** Draft  
**Depends on:** AM3-01  
**Spec refs:** [06-github-issues-sync](../../docs/specs/06-github-issues-sync.md#commands)

## Story

As **Chris (running several agents)**, I want to apply an approved plan, so that GitHub mirrors the backlog.

## Scope

- `issues-sync --apply` for projects with `issues.mode: push`; `--yes-public` guard
- Idempotent; backoff on rate limits

## Acceptance criteria

- [ ] Second run is all `ok`
- [ ] Conflicts are never overwritten

## Test plan

- Against a scratch private repository only

## Notes

Needs Chris's approval per repository (AD-007).

<!-- cspell:words ANSI ChrisC413 OQ Ollama PostToolUse Pro Subagent SubagentStop argparse backlogs cmake ctypes dedupe deduplicated fromisoformat genesisEmu gh gitignored isatty jsonl kernel32 lockfile ms msvcrt npm osascript piggy pipx rate_limits relayed relayer relayers statusLine statusline stdlib subagent subagents subtree superproject termios toolu uncached unittest unterminated wintypes worktree worktrees -->
