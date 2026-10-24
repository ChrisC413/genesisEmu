# AM4-01 · Transcript watcher

**Milestone:** [AM4 – Capture and usage](README.md)  
**Status:** Draft  
**Depends on:** AM0-02  
**Spec refs:** [04-capture-and-usage](../../docs/specs/04-capture-and-usage.md#capture-options-mq7)  
**Was:** genesisEmu T01-11

## Story

As **Chris (running several agents)**, I want liveness, current activity and token usage read from Claude Code transcripts, so that I get them without agents reporting (option D).

## Scope

- Map agent -> `subagents/agent-<id>.meta.json` `worktreePath` -> branch
- Incremental tail of transcripts; last tool call, last assistant line, tokens deduplicated by `requestId`
- Version guard: unknown record shapes ignored and counted

## Acceptance criteria

- [ ] Zero writes outside the monitor; nothing shown beyond one-line summaries
- [ ] Works when transcripts are several MB

## Test plan

- Unit tests on trimmed, anonymised transcript fixtures

## Notes

Format is internal to Claude Code and may change (spec 04).

<!-- cspell:words ANSI ChrisC413 OQ Ollama PostToolUse Pro Subagent SubagentStop argparse backlogs cmake ctypes dedupe deduplicated fromisoformat genesisEmu gh gitignored isatty jsonl kernel32 lockfile ms msvcrt npm osascript piggy pipx rate_limits relayed relayer relayers statusLine statusline stdlib subagent subagents subtree superproject termios toolu uncached unittest unterminated wintypes worktree worktrees -->
