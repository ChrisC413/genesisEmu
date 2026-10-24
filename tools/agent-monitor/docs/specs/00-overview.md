# 00 · Overview

**agent-monitor** lets one person run several coding agents in parallel, across one or more projects,
and stay in control of them:

- **see** what every agent is doing live: project, story, state, last message, liveness, branch, PR, CI;
- **talk** to them: answer their questions, steer them, stop them, without interrupting their work;
- **plan** with them: each project's backlog of milestones and stories, which agent is on which story,
  and optional mirroring of stories to GitHub Issues.

It started inside genesisEmu (`tools/agent-monitor`, genesisEmu spec 17). Its design is
project-agnostic, and [07-repository-and-adoption](07-repository-and-adoption.md) proposes moving it to
its own repository.

## Goals

1. **At a glance:** one row per agent; problems (`blocked`, `failed`, `STALE`, unanswered blocking
   questions, failed CI) stand out without being searched for.
2. **Two-way, low friction:** questions from agents and answers or steering from the user travel through
   the same files, and reach the agent at its next checkpoint without costing it extra tool calls or tokens.
3. **Zero coupling:** agents only append lines to files and read their inbox. A crashed, slow or absent
   monitor never affects an agent; agents never need the monitor running.
4. **Don't hurt agent performance:** no per-tool-call latency, no tokens spent unless there is a message
   to deliver ([04-capture-and-usage](04-capture-and-usage.md)).
5. **Agent-agnostic:** any process can report and receive messages: Claude Code sessions and subagents,
   headless `claude -p` runs, scripts, and **local agents on Ollama** or other non-Claude runtimes. The
   protocol is files plus JSON; the helpers are a CLI and a Python module, and a plain `echo >>` works too.
6. **Multi-project:** every project is tied to a GitHub repository, **private by default**, and declares
   itself with a small registration file.
7. **Cross-platform, near-zero setup:** Windows 11 and macOS 13+, Python 3.9+ standard library only
   ([AD-001](../DECISIONS.md)); `git` and `gh` are optional.
8. **Spec- and story-driven:** this project follows the same discipline as genesisEmu: specs here,
   stories as one file each in milestone folders that each end in a testable stage, decisions in
   [DECISIONS.md](../DECISIONS.md), and open questions with recommended defaults in
   [OPEN-QUESTIONS.md](../OPEN-QUESTIONS.md).

## Non-goals

- Running or scheduling agents. The monitor watches and talks to them; launching them stays with Claude
  Code, scripts, or the user.
- Hard control. A `stop` message is a request the agent honours at its next checkpoint, not a kill.
- Replacing GitHub. PRs, story files and (optionally) issues are the record; the event log is telemetry.
- Hosted service, accounts, or data leaving the machine (except optional GitHub calls through `gh`).

## Priorities (from the user, 2026-10-24)

1. Two-way channel: feedback from agents, answers, steering ([03](03-two-way-channel.md), milestone AM1).
2. Multi-project backlogs ([05](05-multi-project-backlog.md), AM2).
3. GitHub Issues sync ([06](06-github-issues-sync.md), AM3).
4. Capture and usage: transcript watching, plan quota ([04](04-capture-and-usage.md), AM4).
5. Operations: notifications, retention, CI, optional web view (AM5). Then local agents on Ollama and
   multiple machines (AM6, low priority).

## Specs

| Spec | Topic |
|------|-------|
| [00-overview](00-overview.md) | This page |
| [01-event-protocol](01-event-protocol.md) | Status files, v1 line format, extensions, producing events |
| [02-monitor-and-views](02-monitor-and-views.md) | Tailer, state, liveness, git/PR/CI enrichment, terminal views, JSON snapshot |
| [03-two-way-channel](03-two-way-channel.md) | Questions, inbox, answers, steering, delivery paths, routing with Claude Code `SendMessage` |
| [04-capture-and-usage](04-capture-and-usage.md) | How activity is captured (options and their cost to agents); token and plan-usage data on Claude Pro |
| [05-multi-project-backlog](05-multi-project-backlog.md) | Projects, registration file, registry, backlog view, agents per story |
| [06-github-issues-sync](06-github-issues-sync.md) | Optional stories to Issues mirroring: direction, mapping, conflicts, dry run |
| [07-repository-and-adoption](07-repository-and-adoption.md) | Standalone repository proposal, migration plan, how a project adopts the tool |

## Glossary

| Term | Meaning |
|------|---------|
| Agent | Any process doing work and reporting under a name (normally its branch name) |
| Status file | `<status_dir>/<agent>.jsonl`, written only by that agent |
| Inbox | `<status_dir>/<agent>.inbox.jsonl`, written by the user or a relayer, read by the agent |
| Checkpoint | A moment when the agent calls the helper anyway (an event), so pending messages reach it for free |
| Relayer | A process that pushes inbox messages to an agent another way (e.g. a Claude Code coordinator using `SendMessage`) and records the delivery |
| Project | A repository registered with `.agent-project.json`; has its own status dir and stories |

## Testing

`python -m unittest discover -s tools/agent-monitor/tests`: protocol parsing, tailing (partial and
unterminated lines, truncation), state and liveness, emit (including a 4-writer concurrent append), the
two-way channel lifecycle, backlog parsing, and the issues planner. Each story's test plan names its tests.

<!-- cspell:words ANSI ChrisC413 OQ Ollama PostToolUse Pro Subagent SubagentStop argparse backlogs cmake ctypes dedupe deduplicated fromisoformat genesisEmu gh gitignored isatty jsonl kernel32 lockfile ms msvcrt npm osascript piggy pipx rate_limits relayed relayer relayers statusLine statusline stdlib subagent subagents subtree superproject termios toolu uncached unittest unterminated wintypes worktree worktrees -->
