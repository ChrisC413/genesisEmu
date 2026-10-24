# T01-03 · Branch, commit, PR and CI status per agent

**Milestone:** [T01 – Agent monitoring](README.md)  
**Status:** Done  
**Depends on:** T01-02  
**Spec refs:** [17-agent-monitoring](../../docs/specs/17-agent-monitoring.md#architecture) (Architecture)

## Story

As a **developer**, I want each agent's row to show its branch, latest commit, PR and CI result, so that I can tell real progress from chatter.

## Scope

- Resolve branch from `branch` field, `worktree` field, or a worktree whose branch matches the agent name
- `git log -1` per branch (15 s cache) and `gh pr list --head` (90 s cache), off the UI thread in live mode
- Collapse `statusCheckRollup` into pass / fail / pending / none
- `--no-git`, `--no-gh`; `gh` found on PATH, default install paths, or `AGENT_MONITOR_GH`

## Acceptance criteria

- [x] Without git or gh, or offline, the monitor still runs and omits the line
- [x] A UI refresh never waits on a `gh` call
- [x] Worktree porcelain and check-rollup parsing covered by unit tests

## Test plan

- `tests/test_emit_render_git.py::GitParsingTests`
- Manual: snapshot shows `branch m00-foundation | <sha> ...`

## Notes

Failed CI does not yet count as an attention signal (see T01-05).

<!-- cspell:words worktree worktrees gitignored stdlib argparse unittest ctypes lockfile npm osascript jsonl -->
