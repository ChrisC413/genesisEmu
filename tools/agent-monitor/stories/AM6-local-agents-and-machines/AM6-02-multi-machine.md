# AM6-02 · Multi-machine aggregation

**Milestone:** [AM6 – Local agents and multiple machines](README.md)  
**Status:** Draft  
**Depends on:** AM2-02  
**Spec refs:** [01-event-protocol](../../docs/specs/01-event-protocol.md#optional-fields-on-any-event)  
**Was:** genesisEmu T01-10

## Story

As **Chris (running several agents)**, I want agents on my Windows desktop and my Mac in one view, so that I can split work across machines.

## Scope

- `host` field; several status dirs (e.g. a synced folder) or a peer's `--serve` snapshot
- Clock-skew tolerance

## Acceptance criteria

- [ ] Same agent name on two hosts shows two rows

## Test plan

- Unit tests merging two directories

## Notes

—

<!-- cspell:words ANSI ChrisC413 OQ Ollama PostToolUse Pro Subagent SubagentStop argparse backlogs cmake ctypes dedupe deduplicated fromisoformat genesisEmu gh gitignored isatty jsonl kernel32 lockfile ms msvcrt npm osascript piggy pipx rate_limits relayed relayer relayers statusLine statusline stdlib subagent subagents subtree superproject termios toolu uncached unittest unterminated wintypes worktree worktrees -->
