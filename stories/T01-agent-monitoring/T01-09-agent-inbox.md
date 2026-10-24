# T01-09 · Cooperative agent inbox (messages to agents)

**Milestone:** [T01 – Agent monitoring](README.md)  
**Status:** Draft  
**Depends on:** T01-04  
**Spec refs:** [17-agent-monitoring](../../docs/specs/17-agent-monitoring.md#open-questions) (MQ6)

## Story

As a **developer**, I want to leave a message for an agent that it reads between steps, so that I can answer a `blocked` question without stopping it.

## Scope

- `.agent-status/<agent>.inbox.jsonl` format
- `monitor.py send <agent> "text"`
- Hook-driven delivery to the agent; acknowledgement as a normal event

## Acceptance criteria

- [ ] Messages are delivered once and acknowledged
- [ ] Agents without the hook are unaffected

## Test plan

- Unit tests of inbox read/ack bookkeeping

## Notes

Only if the user wants two-way communication (MQ6). Not a command channel: messages are advice, not instructions with elevated authority.

<!-- cspell:words worktree worktrees gitignored stdlib argparse unittest ctypes lockfile npm osascript jsonl -->
