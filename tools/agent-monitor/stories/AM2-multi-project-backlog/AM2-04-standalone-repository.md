# AM2-04 · Move to a standalone private repository

**Milestone:** [AM2 – Multiple projects and backlogs](README.md)  
**Status:** Draft  
**Depends on:** AM0-04  
**Spec refs:** [07-repository-and-adoption](../../docs/specs/07-repository-and-adoption.md#migration-plan-for-when-the-user-approves)

## Story

As **Chris (running several agents)**, I want the tool in its own private repository, so that every project uses one copy at a stable path.

## Scope

- Follow the migration plan (subtree split, new repo, shim in genesisEmu)

## Acceptance criteria

- [ ] History preserved
- [ ] Running agents unaffected
- [ ] genesisEmu keeps only the contract files and a temporary shim

## Test plan

- Tests pass in the new repository's CI

## Notes

Blocked on Chris's decision (AD-006 / OQ-3). Do not create the repository without approval.

<!-- cspell:words ANSI ChrisC413 OQ Ollama PostToolUse Pro Subagent SubagentStop argparse backlogs cmake ctypes dedupe deduplicated fromisoformat genesisEmu gh gitignored isatty jsonl kernel32 lockfile ms msvcrt npm osascript piggy pipx rate_limits relayed relayer relayers statusLine statusline stdlib subagent subagents subtree superproject termios toolu uncached unittest unterminated wintypes worktree worktrees -->
