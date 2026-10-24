# AM1-04 · Coordinator relay through SendMessage

**Milestone:** [AM1 – Two-way channel](README.md)  
**Status:** Draft  
**Depends on:** AM1-03  
**Spec refs:** [03-two-way-channel](../../docs/specs/03-two-way-channel.md#reconciling-with-claude-codes-sendmessage)

## Story

As **Chris (running several agents)**, I want the coordinator session to push urgent messages to its subagents and record that it did, so that answers arrive immediately without double delivery.

## Scope

- Documented relay procedure for the coordinator: `pending --json` -> `SendMessage` -> `mark-delivered`
- Optional `monitor.py relay-text <agent>` that formats pending messages for pasting

## Acceptance criteria

- [ ] A relayed message is not printed again at the agent's next emit
- [ ] The audit trail shows `via: sendmessage`

## Test plan

- Field trial with the M00 agent

## Notes

OQ-8: relay on request only, by default.

<!-- cspell:words ANSI ChrisC413 OQ Ollama PostToolUse Pro Subagent SubagentStop argparse backlogs cmake ctypes dedupe deduplicated fromisoformat genesisEmu gh gitignored isatty jsonl kernel32 lockfile ms msvcrt npm osascript piggy pipx rate_limits relayed relayer relayers statusLine statusline stdlib subagent subagents subtree superproject termios toolu uncached unittest unterminated wintypes worktree worktrees -->
