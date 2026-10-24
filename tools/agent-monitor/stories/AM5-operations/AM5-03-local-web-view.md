# AM5-03 · Read-only local web view

**Milestone:** [AM5 – Operations](README.md)  
**Status:** Draft  
**Depends on:** AM0-02  
**Spec refs:** [02-monitor-and-views](../../docs/specs/02-monitor-and-views.md#modes-and-commands)  
**Was:** genesisEmu T01-07

## Story

As **Chris (running several agents)**, I want an optional browser view on localhost, so that I can keep it on a second screen (AD-004).

## Scope

- `--serve [host:port]` with `http.server`, default `127.0.0.1`
- `/api/snapshot` + one static page

## Acceptance criteria

- [ ] No write endpoints
- [ ] Binding beyond localhost needs an explicit host

## Test plan

- Unit test of the endpoint

## Notes

—

<!-- cspell:words ANSI ChrisC413 OQ Ollama PostToolUse Pro Subagent SubagentStop argparse backlogs cmake ctypes dedupe deduplicated fromisoformat genesisEmu gh gitignored isatty jsonl kernel32 lockfile ms msvcrt npm osascript piggy pipx rate_limits relayed relayer relayers statusLine statusline stdlib subagent subagents subtree superproject termios toolu uncached unittest unterminated wintypes worktree worktrees -->
