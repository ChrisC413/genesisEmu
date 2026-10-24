# 17 · Agent Monitoring (adoption)

genesisEmu is built by several Claude Code agents working in parallel. They report to, and are
supervised with, **agent-monitor**, a separate development tool with its own specs, decisions and
stories:

- tool and docs: [`tools/agent-monitor/`](../../tools/agent-monitor/README.md)
- specs: [`tools/agent-monitor/docs/specs/`](../../tools/agent-monitor/docs/specs/00-overview.md)
- stories: [`tools/agent-monitor/stories/`](../../tools/agent-monitor/stories/README.md)

The tool is proposed to move to its own private repository
([its spec 07](../../tools/agent-monitor/docs/specs/07-repository-and-adoption.md), AD-006; not yet
decided). This spec only covers the **contract genesisEmu keeps** whichever way that is decided.

## Contract

| Item | genesisEmu value |
|------|------------------|
| Registration file | [`.agent-project.json`](../../.agent-project.json): `ChrisC413/genesisEmu`, **private**, stories in `stories/`, Issues sync off |
| Status directory | `.agent-status/` in the main checkout, which all worktrees share; gitignored |
| Event format | v1 JSON Lines (`ts`, `agent`, `story`, `state`, `message`), plus optional extensions ([protocol](../../tools/agent-monitor/docs/specs/01-event-protocol.md)) |
| Agent name | The agent's branch name, e.g. `m00-foundation` |
| Story IDs in events | genesisEmu story IDs (`M00-04`); the tool's own stories use `AM<N>-<NN>` |
| Two-way channel | Questions from agents, and answers or steering from Chris, through `<agent>.inbox.jsonl` ([channel](../../tools/agent-monitor/docs/specs/03-two-way-channel.md)) |

## Instructions for genesisEmu agents

Add the [agent instructions snippet](../../tools/agent-monitor/docs/specs/07-repository-and-adoption.md#agent-instructions-snippet)
to every agent's task prompt. In short:

- report with `emit.py` at story start, at meaningful steps, and at the end;
- when the spec leaves a decision open, ask a question with a default and keep going;
- apply what's printed under `[agent-monitor inbox]` at the next checkpoint.

Worktrees branched before the tool reached `main` don't contain `emit.py`. Use the copy in another
checkout by absolute path, or append the JSON line directly.

<!-- cspell:words worktree worktrees Worktrees gitignored -->
