# Stories (agent-monitor)

One folder per milestone; each ends in a testable stage. One file per story: `AM<N>-<NN>-<slug>.md`, using
[_TEMPLATE.md](_TEMPLATE.md). Status values: `Draft` -> `Ready` -> `In progress` -> `Done`. In this repository
a story is `Done` when its acceptance criteria are met and its tests pass locally; CI arrives with AM5-01.

| Milestone | Goal | Testable stage | Stories |
|-----------|------|----------------|---------|
| [AM0 · MVP: watch agents live](AM0-mvp/README.md) | One terminal view of every agent writing to the status directory. | see README | 4/4 done |
| [AM1 · Two-way channel](AM1-two-way-channel/README.md) | Agents ask, Chris answers and steers, without interrupting anyone (priority 1). | see README | 3/7 done |
| [AM2 · Multiple projects and backlogs](AM2-multi-project-backlog/README.md) | One monitor for several repositories, each with its backlog and the agents on its stories (priority 2). | see README | 2/5 done |
| [AM3 · GitHub Issues sync (optional)](AM3-issues-sync/README.md) | Mirror stories to GitHub Issues one way, safely (priority 3). | see README | 1/4 done |
| [AM4 · Capture and usage](AM4-capture-and-usage/README.md) | Activity, tokens and plan quota without costing agents anything. | see README | 0/3 done |
| [AM5 · Operations](AM5-operations/README.md) | Keep the tool reliable and the view focused. | see README | 0/3 done |
| [AM6 · Local agents and multiple machines](AM6-local-agents-and-machines/README.md) | Non-Claude agents and more than one machine (low priority). | see README | 0/2 done |

<!-- cspell:words ANSI ChrisC413 OQ Ollama PostToolUse Pro Subagent SubagentStop argparse backlogs cmake ctypes dedupe deduplicated fromisoformat genesisEmu gh gitignored isatty jsonl kernel32 lockfile ms msvcrt npm osascript piggy pipx rate_limits relayed relayer relayers statusLine statusline stdlib subagent subagents subtree superproject termios toolu uncached unittest unterminated wintypes worktree worktrees -->
