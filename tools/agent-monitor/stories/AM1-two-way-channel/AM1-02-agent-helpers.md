# AM1-02 · Agent-side helpers: question, piggy-back, check, wait

**Milestone:** [AM1 – Two-way channel](README.md)  
**Status:** Done  
**Depends on:** AM1-01  
**Spec refs:** [03-two-way-channel](../../docs/specs/03-two-way-channel.md#delivery-paths)

## Story

As **Chris (running several agents)**, I want agents to receive my messages without extra tool calls, so that the channel does not cost them performance.

## Scope

- `emit.py question` (auto `q<N>`, options, default, blocking)
- Every `emit.py` call prints and acks pending messages; silent when empty
- `emit.py check`; `emit.py wait --qid Q --timeout S` (exit 3 on timeout)

## Acceptance criteria

- [x] No output and no extra writes when the inbox is empty
- [x] `wait` returns within one poll interval of an answer
- [x] Python API: `ask`, `check_inbox`, `wait_for_answer`

## Test plan

- `tests/test_channel.py`

## Notes

—

<!-- cspell:words ANSI ChrisC413 OQ Ollama PostToolUse Pro Subagent SubagentStop argparse backlogs cmake ctypes dedupe deduplicated fromisoformat genesisEmu gh gitignored isatty jsonl kernel32 lockfile ms msvcrt npm osascript piggy pipx rate_limits relayed relayer relayers statusLine statusline stdlib subagent subagents subtree superproject termios toolu uncached unittest unterminated wintypes worktree worktrees -->
