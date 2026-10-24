# AM0-03 · Branch, commit, PR and CI per agent

**Milestone:** [AM0 – MVP: watch agents live](README.md)  
**Status:** Done  
**Depends on:** AM0-02  
**Spec refs:** [02-monitor-and-views](../../docs/specs/02-monitor-and-views.md#architecture)  
**Was:** genesisEmu T01-03

## Story

As **Chris (running several agents)**, I want each agent's row to show its branch, latest commit, PR and CI result, so that I can tell real progress from chatter.

## Scope

- Branch resolution (field, worktree, branch named like the agent)
- Cached `git log -1` and `gh pr list --head`, async in live mode
- CI rollup collapsed to pass/fail/pending/none

## Acceptance criteria

- [x] Without git/gh or offline, the monitor still runs
- [x] A redraw never waits on `gh`

## Test plan

- `tests/test_emit_render_git.py::GitParsingTests`; manual snapshot shows PR #1

## Notes

—

<!-- cspell:words ANSI ChrisC413 OQ Ollama PostToolUse Pro Subagent SubagentStop argparse backlogs cmake ctypes dedupe deduplicated fromisoformat genesisEmu gh gitignored isatty jsonl kernel32 lockfile ms msvcrt npm osascript piggy pipx rate_limits relayed relayer relayers statusLine statusline stdlib subagent subagents subtree superproject termios toolu uncached unittest unterminated wintypes worktree worktrees -->
