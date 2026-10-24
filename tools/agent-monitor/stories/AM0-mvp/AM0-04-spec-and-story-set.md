# AM0-04 · Standalone spec, decision and story set

**Milestone:** [AM0 – MVP: watch agents live](README.md)  
**Status:** Done  
**Depends on:** AM0-01  
**Spec refs:** [00-overview](../../docs/specs/00-overview.md)

## Story

As **Chris (running several agents)**, I want the monitor to have its own specs, decisions, open questions and milestone stories, so that it is developed with the same discipline as genesisEmu and can move to its own repository.

## Scope

- `docs/specs/00..07`, `docs/DECISIONS.md`, `docs/OPEN-QUESTIONS.md`
- Milestone folders AM0..AM6 with README + one file per story
- genesisEmu spec 17 reduced to the adoption contract

## Acceptance criteria

- [x] Every story has scope, acceptance criteria, test plan, dependencies and spec refs
- [x] Every milestone README states a testable stage

## Test plan

- Review in PR #1

## Notes

Replaces genesisEmu `stories/T01-agent-monitoring` (T01-01..12 renumbered; see each story's *Was*).

<!-- cspell:words ANSI ChrisC413 OQ Ollama PostToolUse Pro Subagent SubagentStop argparse backlogs cmake ctypes dedupe deduplicated fromisoformat genesisEmu gh gitignored isatty jsonl kernel32 lockfile ms msvcrt npm osascript piggy pipx rate_limits relayed relayer relayers statusLine statusline stdlib subagent subagents subtree superproject termios toolu uncached unittest unterminated wintypes worktree worktrees -->
