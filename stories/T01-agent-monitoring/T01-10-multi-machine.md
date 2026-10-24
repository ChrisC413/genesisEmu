# T01-10 · Multi-machine aggregation

**Milestone:** [T01 – Agent monitoring](README.md)  
**Status:** Draft  
**Depends on:** T01-02  
**Spec refs:** [17-agent-monitoring](../../docs/specs/17-agent-monitoring.md#open-questions) (MQ4)

## Story

As a **developer**, I want to see agents running on my Windows desktop and my Mac in one view, so that I can split work across machines.

## Scope

- `host` field from the emit helper
- Multiple `--dir` sources (e.g. a synced folder)
- Clock-skew tolerance

## Acceptance criteria

- [ ] Same agent name on two hosts shows as two rows

## Test plan

- Unit tests merging two directories

## Notes

Deferred until agents actually run on both machines.

<!-- cspell:words worktree worktrees gitignored stdlib argparse unittest ctypes lockfile npm osascript jsonl -->
