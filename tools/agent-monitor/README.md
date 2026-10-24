# agent-monitor

Watch, question and steer coding agents working in parallel across your projects, **without
interrupting them**. Agents append one-line JSON events to `.agent-status/<agent>.jsonl` in the main
checkout. The user (or a coordinator session) leaves answers and steering in `<agent>.inbox.jsonl`,
and agents pick them up at their next report.

Python 3.9+ standard library only (developed on 3.13). Windows 11 and macOS 13+. `git` and `gh` are
optional. Any agent runtime can take part (Claude Code, scripts, local Ollama agents); the protocol is
plain files.

This is its own spec- and story-driven project:

- [specs](docs/specs/00-overview.md)
- [decisions](docs/DECISIONS.md)
- [open questions](docs/OPEN-QUESTIONS.md)
- [stories](stories/README.md), milestones AM0 to AM6

It is proposed to move to its own private repository
([spec 07](docs/specs/07-repository-and-adoption.md)).

## Watch

```sh
python tools/agent-monitor/monitor.py            # live view, refreshes every second, Ctrl+C to quit
python tools/agent-monitor/monitor.py --once     # one plain-text snapshot (works when piped)
python tools/agent-monitor/monitor.py --json     # one machine-readable snapshot
```

Options:

- `--stale-minutes 15`, `--warn-minutes 5`, `--feed 15`
- `--no-gh` (skip PR/CI lookups), `--no-git`
- `--dir <path>`, `--color never`

The status directory defaults to `$AGENT_STATUS_DIR`, otherwise `<main checkout>/.agent-status`, so
the tool works from any worktree.

The `LIVE` column shows one of these:

| Value | Meaning |
|-------|---------|
| `ASK` | Unanswered blocking question |
| `QUIET` | No event for 5 min |
| `STALE` | No event for 15 min |

Agents that are done or have failed are never marked stale.

## Talk to agents

```sh
python tools/agent-monitor/monitor.py questions                         # open questions
python tools/agent-monitor/monitor.py answer m00-foundation q1 "Use REV01"
python tools/agent-monitor/monitor.py steer  m00-foundation "Do M00-06 before M00-05"
python tools/agent-monitor/monitor.py note   m00-foundation "CI on main is red, not your fault"
python tools/agent-monitor/monitor.py stop   m00-foundation "Wrap up after this story"
python tools/agent-monitor/monitor.py inbox  m00-foundation             # messages and delivery state
python tools/agent-monitor/monitor.py pending --json                    # for a coordinator relaying via SendMessage
python tools/agent-monitor/monitor.py mark-delivered m00-foundation m-20261024... --via sendmessage
```

Messages are delivered at the agent's next `emit.py` call, so they cost the agent no extra tool calls.
A coordinator can also relay them with `SendMessage` and mark them delivered
([spec 03](docs/specs/03-two-way-channel.md)).

## Backlogs and Issues

```sh
python tools/agent-monitor/monitor.py backlog                 # this repo's milestones/stories + which agent is on what
python tools/agent-monitor/monitor.py backlog --project tools/agent-monitor
python tools/agent-monitor/monitor.py issues-plan --offline   # DRY RUN of a stories->Issues sync; never writes
```

Projects declare themselves with `.agent-project.json` (GitHub repo, **private** by default,
story layout, Issues mode); see [spec 05](docs/specs/05-multi-project-backlog.md).

## Report and ask (for agents)

```sh
python tools/agent-monitor/emit.py -a m00-foundation -s M00-04 started  "ROM header parser"
python tools/agent-monitor/emit.py -a m00-foundation -s M00-04 progress "Checksum + region decoded" -p 60
python tools/agent-monitor/emit.py -a m00-foundation -s M00-04 question "Which S2 revision for the test?" --option REV00 --option REV01 --default REV01
python tools/agent-monitor/emit.py -a m00-foundation -s M00-04 question "OK to vendor zlib?" --blocking   # prints qid=q2
python tools/agent-monitor/emit.py -a m00-foundation wait --qid q2 --timeout 300                          # exit 3 on timeout
python tools/agent-monitor/emit.py -a m00-foundation check                                                # explicit inbox check
python tools/agent-monitor/emit.py -a m00-foundation -s M00-04 done     "12 header tests pass"
```

Any pending inbox messages are printed after each event under `[agent-monitor inbox]`; nothing is
printed when the inbox is empty. `AGENT_NAME` and `AGENT_STORY` can replace `-a` and `-s`. The helper
adds `branch` and `worktree` (disable with `--no-git`) and appends atomically. Name the agent after its
branch. The text for agent prompts is in [spec 07](docs/specs/07-repository-and-adoption.md#agent-instructions-snippet).

Any writer that produces the v1 format also works:

```sh
echo '{"ts":"2026-10-24T11:22:33Z","agent":"m00-foundation","story":"M00-04","state":"progress","message":"..."}' >> .agent-status/m00-foundation.jsonl
```

## Test

```sh
python -m unittest discover -s tools/agent-monitor/tests -v
```

## Layout

| Path | Role |
|------|------|
| `agent_monitor/protocol.py` | Event v1, extensions, side states, line parser |
| `agent_monitor/tail.py` | Polling tail, partial-line buffering, truncation handling |
| `agent_monitor/state.py` | Per-agent state, liveness, questions, inbox delivery state, feed |
| `agent_monitor/inbox.py` | Inbox messages: post, pending, mark delivered, agent-facing text |
| `agent_monitor/emit.py` | `emit.py`: report, ask, check, wait |
| `agent_monitor/commands.py` | `monitor.py` subcommands |
| `agent_monitor/project.py`, `backlog.py` | Registration file, registry, story and milestone parsing |
| `agent_monitor/issues.py` | Dry-run Issues sync planner |
| `agent_monitor/gitinfo.py`, `render.py`, `cli.py` | git/gh enrichment, rendering, live view |
| `docs/`, `stories/` | This project's specs, decisions, open questions, milestones |

<!-- cspell:words ANSI ChrisC413 OQ Ollama PostToolUse Pro Subagent SubagentStop argparse backlogs cmake ctypes dedupe deduplicated fromisoformat genesisEmu gh gitignored isatty jsonl kernel32 lockfile ms msvcrt npm osascript piggy pipx rate_limits relayed relayer relayers statusLine statusline stdlib subagent subagents subtree superproject termios toolu uncached unittest unterminated wintypes worktree worktrees -->
