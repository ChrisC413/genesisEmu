# AM3-03 · Issue comments as feedback

**Milestone:** [AM3 – GitHub Issues sync (optional)](README.md)  
**Status:** Draft  
**Depends on:** AM3-02  
**Spec refs:** [06-github-issues-sync](../../docs/specs/06-github-issues-sync.md#direction-proposal)

## Story

As **Chris (running several agents)**, I want comments on story issues to show next to the story and optionally reach the agent on it, so that feedback given on GitHub isn't lost.

## Scope

- Read comments since last sync; show in backlog view
- `--forward` posts them as `note` inbox messages to the agent on that story

## Acceptance criteria

- [ ] Each comment forwarded at most once

## Test plan

- Unit tests with recorded `gh` output

## Notes

—

<!-- cspell:words ANSI ChrisC413 OQ Ollama PostToolUse Pro Subagent SubagentStop argparse backlogs cmake ctypes dedupe deduplicated fromisoformat genesisEmu gh gitignored isatty jsonl kernel32 lockfile ms msvcrt npm osascript piggy pipx rate_limits relayed relayer relayers statusLine statusline stdlib subagent subagents subtree superproject termios toolu uncached unittest unterminated wintypes worktree worktrees -->
