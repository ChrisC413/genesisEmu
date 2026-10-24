# 02 · Monitor and Views

## Architecture

```
 agents (any runtime)          <status_dir>/                     monitor
 ┌───────────────┐ emit.py     ┌──────────────────────────┐ poll ┌──────────┐  ┌──────────┐  ┌────────────┐
 │ m00-foundation├────────────►│ m00-foundation.jsonl     ├─────►│ DirTailer├─►│ Monitor  ├─►│ renderers  │
 │ agent-monitor │◄── prints ──┤ m00-foundation.inbox.jsonl│ 1 s  │          │  │ agents,  │  │ live, once │
 │ ollama-runner │   inbox     │ …                        │      └──────────┘  │ questions│  │ json, cmds │
 └───────────────┘             └──────────▲───────────────┘                    └────┬─────┘  └────────────┘
                                          │ monitor.py answer / steer / note / stop  │
                                       user / relayer             git worktree list · git log · gh pr list
```

- **Tailer:** polls the directory every second, picks up new files, and reads only the bytes appended
  since the last poll.
  - It returns complete lines only. A partial trailing line is held back until its newline arrives.
  - If a partial line stays unchanged for 3 s *and* is a complete JSON object, it is accepted once.
  - A file that shrinks is read again from the start.
  - It polls instead of using OS file events, which keeps Windows and macOS identical and needs no
    dependencies ([AD-002](../DECISIONS.md)).
- **State:** one `AgentState` per agent.
  - The newest event by `ts` that isn't a side event sets the agent's story, state and message.
    Older events that arrive out of order go to the feed only.
  - Questions, acks and inbox messages make up the [two-way channel](03-two-way-channel.md) state.
  - Malformed lines are counted, and the latest few are shown with file and line; they never stop
    processing.
- **Enrichment (optional):** the branch is resolved in this order:
  1. the event's `branch` field;
  2. the worktree whose path matches the `worktree` field;
  3. a worktree on a branch named after the agent.

  The monitor then shows that branch's latest commit (`git log -1`, cached 15 s) and its PR with a
  collapsed CI result `pass`/`fail`/`pending`/`none` (`gh pr list --head`, cached 90 s). In live mode
  these lookups run on a background thread, and any failure just leaves the line out.

## Liveness

| LIVE | Rule |
|------|------|
| *(blank)* | Last event less than 5 min ago |
| `QUIET` | No event for 5 min (`--warn-minutes`) |
| `STALE` | No event for 15 min (`--stale-minutes`) |
| `ASK` | Has an unanswered **blocking** question. This takes precedence over QUIET and STALE. |
| *(idle)* | Last state is `done` or `failed`. Silence is expected, so no warning is shown. |

Timestamps in the future (clock skew) count as age 0. Thresholds are global for now (OQ-1).

## Views

**Agent table**, most recently active first:

| Column | Content |
|--------|---------|
| AGENT | Agent name |
| STORY | Latest story ID |
| STATE | Latest state that isn't a side event, colour-coded |
| AGE | Time since the newest event of any kind |
| LIVE | See the liveness table |
| MESSAGE | `[ 60%]` prefix when `percent` is set, then the last message |

Under each row:

- the git line `branch <name> | PR #12 draft open CI pass | <sha> <subject> (<age>)` (the commit
  subject is cut first when space runs out);
- `done: <stories>`, listing the stories this agent has finished.

**Open questions:** blocking questions first. Each shows its age, options and default, plus "answered,
awaiting pickup" once an answer is queued. Below them, a line names agents whose inbox has messages
they haven't picked up yet.

**Recent events:** the newest N events across agents, heartbeats and acks excluded. In live mode the
feed shrinks to fit the terminal so the newest events stay visible.

**Malformed lines:** a count plus the latest three, shown as `file:line: reason`.

## Modes and commands

| Command | Purpose |
|---------|---------|
| `monitor.py` | Live full-screen view: ANSI, alternate screen buffer, VT mode turned on for Windows consoles. Refuses to start when the output isn't a real console. |
| `monitor.py --once` / `--json` | One text snapshot, or one JSON snapshot that includes `open_questions` and `pending_messages` |
| `monitor.py answer/steer/note/stop/questions/inbox/pending/mark-delivered` | Two-way channel ([03](03-two-way-channel.md)) |
| `monitor.py backlog [--project P] [--all]` | Backlog ([05](05-multi-project-backlog.md)) |
| `monitor.py issues-plan [--project P]` | Dry-run Issues plan ([06](06-github-issues-sync.md)) |

Output is ASCII, so redirected output works with any Windows code page.
