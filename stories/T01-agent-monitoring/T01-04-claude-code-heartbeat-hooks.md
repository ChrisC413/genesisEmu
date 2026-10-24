# T01-04 · Automatic heartbeats from Claude Code hooks

**Milestone:** [T01 – Agent monitoring](README.md)  
**Status:** Draft  
**Depends on:** T01-01  
**Spec refs:** [17-agent-monitoring](../../docs/specs/17-agent-monitoring.md#open-questions) (MQ7, MQ11)

## Story

As a **developer**, I want liveness reported automatically by Claude Code hooks, so that a stuck or dead agent is detected even if it forgets to report.

## Scope

- Project `.claude/settings.json` hook on `PostToolUse` (throttled to 1/min per agent) and `Stop`/`SubagentStop`
- Agent name derived from the worktree branch; `session_id` included
- Hook script in `tools/agent-monitor/` (Python, stdlib)

## Acceptance criteria

- [ ] An agent that never calls `emit.py` still shows a current AGE while it works
- [ ] `Stop` produces a final heartbeat with the reason when available
- [ ] Hook adds < 100 ms per tool call and never fails the tool call

## Test plan

- Unit test of the throttle and name derivation; manual run with a background agent

## Notes

Needs the user's go-ahead: hooks change every agent's behavior in this repo.

<!-- cspell:words worktree worktrees gitignored stdlib argparse unittest ctypes lockfile npm osascript jsonl -->
