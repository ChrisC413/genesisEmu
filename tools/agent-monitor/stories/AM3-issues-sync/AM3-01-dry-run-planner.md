# AM3-01 · Dry-run sync planner

**Milestone:** [AM3 – GitHub Issues sync (optional)](README.md)  
**Status:** Done  
**Depends on:** AM2-01  
**Spec refs:** [06-github-issues-sync](../../docs/specs/06-github-issues-sync.md)

## Story

As **Chris (running several agents)**, I want to see exactly what a stories->issues sync would do before anything is written, so that I can decide whether to enable it.

## Scope

- Marker-based identity, managed labels, milestone mapping, close/reopen, conflicts, orphans
- Read-only `gh issue list` / `gh api .../milestones`; `--offline`

## Acceptance criteria

- [x] No code path writes to GitHub
- [x] Unmanaged labels are never touched

## Test plan

- `tests/test_backlog.py::IssuesPlanTests`

## Notes

—

<!-- cspell:words ANSI ChrisC413 OQ Ollama PostToolUse Pro Subagent SubagentStop argparse backlogs cmake ctypes dedupe deduplicated fromisoformat genesisEmu gh gitignored isatty jsonl kernel32 lockfile ms msvcrt npm osascript piggy pipx rate_limits relayed relayer relayers statusLine statusline stdlib subagent subagents subtree superproject termios toolu uncached unittest unterminated wintypes worktree worktrees -->
