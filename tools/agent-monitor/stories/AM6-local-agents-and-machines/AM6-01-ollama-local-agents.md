# AM6-01 · Local agents on Ollama

**Milestone:** [AM6 – Local agents and multiple machines](README.md)  
**Status:** Draft  
**Depends on:** AM1-02  
**Spec refs:** [00-overview](../../docs/specs/00-overview.md#goals)

## Story

As **Chris (running several agents)**, I want agents running on local Ollama models to report and receive messages like Claude agents, so that I can mix runtimes (MQ4 requirement).

## Scope

- Minimal runner/wrapper that emits started/progress/done/failed around an agent loop and polls the inbox between steps (option C)
- `runtime` field (`ollama:<model>`)

## Acceptance criteria

- [ ] No Claude-specific assumptions in protocol or helpers
- [ ] Works with plain `echo >>` producers too

## Test plan

- Unit tests with a fake agent loop

## Notes

OQ-10: which Ollama agent loop.

<!-- cspell:words ANSI ChrisC413 OQ Ollama PostToolUse Pro Subagent SubagentStop argparse backlogs cmake ctypes dedupe deduplicated fromisoformat genesisEmu gh gitignored isatty jsonl kernel32 lockfile ms msvcrt npm osascript piggy pipx rate_limits relayed relayer relayers statusLine statusline stdlib subagent subagents subtree superproject termios toolu uncached unittest unterminated wintypes worktree worktrees -->
