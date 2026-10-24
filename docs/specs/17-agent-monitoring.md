# 17 · Agent Monitoring (adoption)

genesisEmu is built by several Claude Code agents working in parallel. They report to, and are
supervised with, **agent-monitor**, a separate development tool in its own private repository:
**[ChrisC413/agent-monitor](https://github.com/ChrisC413/agent-monitor)**. That repository has its own
specs, decisions, stories (milestones AM0–AM6) and CI. It moved out of `tools/agent-monitor/` on
2026-10-24 (D-024).

This spec only covers the **contract genesisEmu keeps**.

## Contract

| Item | genesisEmu value |
|------|------------------|
| Registration file | [`.agent-project.json`](../../.agent-project.json): `ChrisC413/genesisEmu`, **private**, stories in `stories/`, Issues sync off |
| Status directory | `.agent-status/` in the main checkout, which all worktrees share; gitignored |
| Event format | v1 JSON Lines: `ts`, `agent`, `story`, `state`, `message`, plus optional extensions (agent-monitor spec 01). Writing lines directly with `echo >>` stays valid. |
| Agent name | The agent's branch name, e.g. `m00-foundation` |
| Story IDs in events | genesisEmu story IDs (`M00-04`) |
| Two-way channel | Questions from agents; answers and steering from Chris through `.agent-status/<agent>.inbox.jsonl` (agent-monitor spec 03) |

## Where the tool is

- **Canonical clone:** `C:\Users\chris\Desktop\projects\agent-monitor`, next to genesisEmu. It's
  optionally named by the `AGENT_MONITOR` environment variable, which isn't set by default.
- **Shim:** `tools/agent-monitor/emit.py` and `monitor.py` in this repository forward to that clone,
  using `$AGENT_MONITOR` or the sibling folder. Existing prompts keep working. The shim will be removed
  after the next milestone.

## Instructions for genesisEmu agents

Use the agent instructions snippet from agent-monitor spec 07 in every agent's task prompt. In short:

- report with `emit.py` at story start, at meaningful steps, and at the end;
- when the spec leaves a decision open, ask with a default and keep going;
- apply what's printed under `[agent-monitor inbox]` at the next checkpoint.

```sh
python C:/Users/chris/Desktop/projects/agent-monitor/emit.py -a <branch> -s <story> progress "<one line>"
python tools/agent-monitor/emit.py -a <branch> -s <story> progress "<one line>"     # via the shim
```

<!-- cspell:words worktree worktrees Worktrees gitignored -->
