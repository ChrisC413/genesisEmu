# AM4-02 · Plan usage from the status line

**Milestone:** [AM4 – Capture and usage](README.md)  
**Status:** Draft  
**Depends on:** AM0-02  
**Spec refs:** [04-capture-and-usage](../../docs/specs/04-capture-and-usage.md#usage-and-quota-on-claude-pro-mq8)

## Story

As **Chris (running several agents)**, I want my remaining Pro quota and reset times in the monitor, so that I can pace parallel agents.

## Scope

- Status line script that prints a normal status line and copies `rate_limits` to `_usage.json`
- Header line with age of the data

## Acceptance criteria

- [ ] Never calls undocumented endpoints (AD-009)
- [ ] Missing/old data shown as such

## Test plan

- Unit test with a sample status line payload

## Notes

OQ-9 (needs consent to add a statusLine command). Check whether the VS Code extension runs it.

<!-- cspell:words ANSI ChrisC413 OQ Ollama PostToolUse Pro Subagent SubagentStop argparse backlogs cmake ctypes dedupe deduplicated fromisoformat genesisEmu gh gitignored isatty jsonl kernel32 lockfile ms msvcrt npm osascript piggy pipx rate_limits relayed relayer relayers statusLine statusline stdlib subagent subagents subtree superproject termios toolu uncached unittest unterminated wintypes worktree worktrees -->
