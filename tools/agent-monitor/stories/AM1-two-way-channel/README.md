# AM1 · Two-way channel

**Goal:** Agents ask, Chris answers and steers, without interrupting anyone (priority 1).

**Depends on:** AM0

## Testable stage (exit criteria)

An agent asks a blocking question and runs `emit.py wait`; Chris runs `monitor.py answer <agent> q1 "..."`; the agent receives it within seconds. A `steer` reaches another agent at its next `emit.py` call. A message relayed by the coordinator with `SendMessage` and marked delivered is not shown to the agent again.

## Stories

| ID | Story | Depends on | Status |
|----|-------|------------|--------|
| AM1-01 | [Question, ack and inbox protocol](AM1-01-question-and-inbox-protocol.md) | AM0-01 | Done |
| AM1-02 | [Agent-side helpers: question, piggy-back, check, wait](AM1-02-agent-helpers.md) | AM1-01 | Done |
| AM1-03 | [User CLI and open-questions panel](AM1-03-user-cli-and-question-panel.md) | AM1-01 | Done |
| AM1-04 | [Coordinator relay through SendMessage](AM1-04-coordinator-relay.md) | AM1-03 | Draft |
| AM1-05 | [Agent contract rollout and field trial](AM1-05-agent-contract-rollout.md) | AM1-02 | Ready |
| AM1-06 | [Answer and steer from the live view](AM1-06-in-view-answering.md) | AM1-03 | Draft |
| AM1-07 | [Attention signals and notifications](AM1-07-attention-and-notifications.md) | AM1-03, AM0-03 | Draft |

<!-- cspell:words ANSI ChrisC413 OQ Ollama PostToolUse Pro Subagent SubagentStop argparse backlogs cmake ctypes dedupe deduplicated fromisoformat genesisEmu gh gitignored isatty jsonl kernel32 lockfile ms msvcrt npm osascript piggy pipx rate_limits relayed relayer relayers statusLine statusline stdlib subagent subagents subtree superproject termios toolu uncached unittest unterminated wintypes worktree worktrees -->
