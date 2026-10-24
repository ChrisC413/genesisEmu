# 17 · Agent Monitoring

Development tooling, not part of the emulator. Several Claude Code agents work on this repository in
parallel, each in its own git worktree and branch. This spec defines how they report what they are doing
and how the user watches them **live** — which story, what state, recent messages, progress, problems —
without interrupting them. Implementation: [`tools/agent-monitor/`](../../tools/agent-monitor/README.md),
stories: [T01](../../stories/T01-agent-monitoring/README.md), technology decision: D-023.

## Goals

1. **At a glance:** one row per agent showing its current story, state, last message, and time since its
   last event.
2. **Problems surface by themselves:** `blocked` and `failed` stand out; an agent that has gone silent
   is flagged `QUIET`, then `STALE`.
3. **Zero coupling:** agents only append lines to a file. A crashed, slow or absent monitor never affects
   an agent, and agents never need the monitor installed.
4. **Repository context:** next to each agent, its branch, latest commit, PR, and CI status.
5. **Cross-platform, near-zero setup:** Windows 11 and macOS 13+, no system software to install.
6. **Stable protocol:** the line format already in use (v1) stays valid forever; extensions are additive.

## Non-goals

- Controlling or interrupting agents (see open question MQ6 for a cooperative inbox).
- Replacing GitHub as the record of work: the event log is ephemeral telemetry; PRs and story files are
  the source of truth.
- Hosted dashboards, accounts, or anything that sends data off the machine.
- Precise token and cost accounting in v1 (MQ8).

## Event protocol

### Location and files

- Directory: `.agent-status/` at the root of the **main checkout** (not inside a worktree, so all
  worktrees share it). It is gitignored. Tools find it via `git rev-parse --git-common-dir`; the
  `AGENT_STATUS_DIR` environment variable overrides it.
- One file per agent: `<agent>.jsonl`. **One writer per file**, append-only.
- The agent name is `[A-Za-z0-9][A-Za-z0-9._-]{0,63}` and **should equal the agent's branch name**
  (e.g. `m00-foundation`), which lets the monitor find the branch, commits, and PR without extra fields.

### v1 line format (frozen)

UTF-8 JSON Lines: one JSON object per line, terminated by `\n`.

```json
{"ts":"2026-10-24T11:22:33Z","agent":"m00-foundation","story":"M00-04","state":"progress","message":"Checksum decoded"}
```

| Field | Type | Required | Meaning |
|-------|------|----------|---------|
| `ts` | string | yes | ISO-8601 time of the event, UTC with a `Z` suffix (readers also accept offsets and treat naive times as UTC) |
| `agent` | string | yes¹ | Agent name; equals the file stem |
| `story` | string | no | Story ID being worked on (`M00-04`, `T01-02`); empty between stories |
| `state` | string | yes | `started` · `progress` · `blocked` · `done` · `failed` |
| `message` | string | no | One human-readable line (readers collapse whitespace/newlines) |

¹ Readers fall back to the file stem when it is missing.

State meanings: `started` = began a story; `progress` = a meaningful step finished; `blocked` = cannot
continue without help (the message says what is needed); `done` = the story's acceptance criteria are
met; `failed` = gave up on the story. `done`/`failed` are **terminal**: silence afterwards is expected.

### Extensions (proposed, backward compatible)

All optional. Readers **must ignore unknown fields** and show unknown states verbatim, so v1 readers
keep working and producers may adopt extensions one at a time. A breaking change would add `"v": 2`;
none is planned.

| Field / value | Type | Meaning |
|---------------|------|---------|
| `state: "heartbeat"` | — | Liveness only: updates "last seen" but not the displayed state/message, and is hidden from the feed. Emit at least every 5 minutes during long builds or test runs. |
| `percent` | int 0–100 | Progress within the current story (reset when the story changes) |
| `branch`, `worktree` | string | Git context; added automatically by the emit helper |
| `pr` | int | PR number the agent opened |
| `files_changed` | int | Files changed on the branch vs. its base |
| `tests` | object | e.g. `{"passed": 12, "failed": 0}` |
| `tokens`, `cost_usd` | int, number | Usage so far, when the producer can obtain it (MQ8) |
| `session_id` | string | Claude Code session ID (lets a hook-based producer and the agent share a file) |
| `host` | string | Machine name, for multi-machine aggregation (MQ4) |
| `detail` | string | Longer free text; not shown in the table |

### Producing events

- **Preferred:** `python tools/agent-monitor/emit.py -a <agent> -s <story> <state> "<message>"` (or the
  `emit()` function). It validates the name and state, formats `ts`, collapses the message to one line,
  adds `branch`/`worktree`, and appends atomically.
- **Fallback:** any tool that appends a correct v1 line (`echo ... >>`).
- **Atomicity:** each event is written with one append call. On Windows the C runtime's `O_APPEND`
  is seek-then-write and loses lines under concurrent writers (observed: 182 of 200 lines survived a
  4-writer stress test), so the helper opens a `FILE_APPEND_DATA` handle there; on POSIX it uses
  `O_APPEND`. "One writer per file" makes this a safety margin rather than a requirement.

## Architecture

```
 agents (worktrees)            .agent-status/                 monitor (tools/agent-monitor)
 ┌──────────────┐  emit.py     ┌─────────────────────┐ poll  ┌────────────┐   ┌───────────┐   ┌────────────┐
 │ m00-foundation├────────────►│ m00-foundation.jsonl├──────►│ DirTailer  ├──►│ Monitor   ├──►│ renderers  │
 │ agent-monitor ├────────────►│ agent-monitor.jsonl │ 1 s   │ (partial-  │   │ per-agent │   │ terminal   │
 │ …             │  (append)   │ …                   │       │  line safe)│   │ state+feed│   │ --once     │
 └──────────────┘              └─────────────────────┘       └────────────┘   └─────┬─────┘   │ --json     │
                                                                                     │         └────────────┘
                                                git worktree list / git log / gh pr list (cached, async)
```

- **Tailer:** polls the directory every second (new files are picked up), reads only appended bytes, and
  returns complete lines. A partial trailing line is buffered until its newline arrives; if it stays
  unchanged for 3 s *and* is a complete JSON object it is accepted (a producer that forgot the final
  newline is not invisible). A file that shrinks is re-read from the start. Polling instead of OS file
  events keeps Windows and macOS identical and dependency-free; a few small files at 1 Hz cost nothing.
- **Monitor state:** one `AgentState` per agent. The newest non-heartbeat event (by `ts`) sets story,
  state and message; older out-of-order events go to the feed only. Malformed lines are counted and the
  latest few shown with file and line number; they never stop processing.
- **Enrichment (optional):** branch resolution, in order: the event's `branch` field → the worktree whose
  path matches `worktree` → a worktree checked out on a branch named like the agent. Then
  `git log -1` for that branch (TTL 15 s) and `gh pr list --head <branch>` for PR number, draft state,
  and a collapsed CI result `pass`/`fail`/`pending`/`none` (TTL 90 s). `gh` is found on `PATH`, then at
  its default install paths, or via `AGENT_MONITOR_GH`. In live mode lookups run on a background thread
  so the screen never freezes; any failure just omits the line.
- **Renderers:** a live full-screen terminal view (ANSI, alternate screen buffer; VT mode enabled on
  Windows consoles), `--once` plain-text snapshot, and `--json` snapshot for other tools (a future web
  view, scripts, or an agent checking on its peers). Output is ASCII so redirected output works on any
  Windows code page.

### Technology choice (D-023, proposed)

**Python 3 standard library.** Python 3.13 is already installed on the Windows machine and macOS 13's Command
Line Tools ship Python 3.9, which the code supports; the stdlib covers everything needed (JSON, argparse, subprocess,
threading, unittest, ctypes for the Windows console and atomic append), so there is nothing to install
and no lockfile to maintain. `curses` is unavailable on Windows, so the UI is plain ANSI redraw, which
Windows Terminal and macOS Terminal both support. Node was the alternative: equally available, but a
comfortable TUI needs npm packages (ink/blessed), and a dependency-free Node version would be the same
ANSI code with a less convenient test story. The repo's existing tooling (`tools/gen_stories.py`) is
Python too. A browser view, if wanted (MQ1), can be served by `http.server` from the same process.

## Views

**Agent table** (most recently active first):

| Column | Content |
|--------|---------|
| AGENT | Agent name |
| STORY | Latest story ID |
| STATE | Latest non-heartbeat state, color-coded (blocked yellow, failed red, done blue) |
| AGE | Time since the newest event of any kind, heartbeats included |
| LIVE | blank = fine; `QUIET` after 5 min; `STALE` after 15 min (`--warn-minutes`, `--stale-minutes`); never for done/failed agents. Future timestamps (clock skew) count as age 0. |
| MESSAGE | `[ 60%]` prefix when `percent` is present, then the last message |

Under each row: `branch <name> | <sha> <subject> (<age>) | PR #12 draft open  CI pass`, and the list of
stories that agent has finished (`done: M00-01, M00-02`).

**Recent events:** the newest N events across all agents, by time (heartbeats excluded). In live mode the
feed shrinks to fit the terminal so the newest events stay visible.

**Malformed lines:** a count plus the latest three, as `file:line: reason`.

Example (`--once`, 2026-10-24):

```
agent-monitor  2026-10-24 11:26:55  2 agent(s)  C:\Users\chris\Desktop\projects\genesisEmu\.agent-status

AGENT              STORY    STATE     AGE      LIVE   MESSAGE
agent-monitor      T01-01   progress  1s              Protocol, tailer, state, render, emit modules written
  branch agent-monitoring | 5da83ab Ignore live agent status directory (5 minutes ago)
m00-foundation     M00-01   started   3m04s           Starting repo layout and CMake/vcpkg build
  branch m00-foundation | 5da83ab Ignore live agent status directory (5 minutes ago)

RECENT EVENTS (last 15)
11:23:27 agent-monitor      T01-01   started   Reading repo; designing event protocol and monitor
11:23:51 m00-foundation     M00-01   started   Starting repo layout and CMake/vcpkg build
11:26:54 agent-monitor      T01-01   progress  Protocol, tailer, state, render, emit modules written
```

## Testing

`python -m unittest discover -s tools/agent-monitor/tests`: parser (v1, extensions, malformed and
partial lines, timestamps), tailer (incremental reads, partial last line, unterminated line after the
grace period without duplication, CRLF, invalid UTF-8, truncation, new files), state (multiple agents,
story transitions, heartbeat, out-of-order events, quiet/stale/idle), emit (round trip, validation,
4-writer concurrent append), renderer (ASCII-only, width, key content), and gh/git output parsing.
Running these in CI is story T01-12 (after M00-02 creates the workflow).

## Open questions

Recommended defaults are what the MVP does today or what the next stories would assume.

| # | Question | Recommended default |
|---|----------|---------------------|
| MQ1 | **Terminal or browser UI?** | Terminal first (done). Add an optional read-only `--serve` mode (stdlib `http.server`, bound to `127.0.0.1`) that serves the `--json` snapshot plus one static HTML page, for a second screen or a phone on the LAN only if explicitly bound wider. No framework. |
| MQ2 | **Should it read git/PR/CI state via `gh`?** | Yes, on by default with caching (90 s TTL — far below API limits), opt out with `--no-gh`. Add *failed CI* to the attention signals. Later: show the failing job name and a link. |
| MQ3 | **Notifications?** | Notify only on transitions into `blocked`, `failed`, and `STALE`, plus `done` of a whole milestone. Terminal bell always; OS notifications opt-in (`--notify`): Windows toast via PowerShell, macOS `osascript`. No sound by default. |
| MQ4 | **Multi-machine (Windows desktop + Mac)?** | Out of scope until agents actually run on both. Then: add `host`, point `AGENT_STATUS_DIR` at a synced folder (one writer per file avoids sync conflicts), or have each monitor read a peer's `--serve` JSON. Not git — too slow and noisy. |
| MQ5 | **Retention?** | Keep everything (lines are ~200 bytes; a busy agent writes a few hundred a day). Hide agents whose last event is older than 7 days (`--since`). When an agent's worktree is removed, move its file to `.agent-status/archive/`. Revisit if any file exceeds 10 MB. |
| MQ6 | **Should the monitor send messages/commands to agents?** | Not in v1: read-only keeps goal 3 intact. If wanted: a cooperative **inbox** — `.agent-status/<agent>.inbox.jsonl`, written by the user or monitor, read by the agent between steps (enforced by a hook), with replies as normal events. The ideal use is answering a `blocked` question. Claude Code's own parent session can already message its background agents; the inbox covers agents it didn't spawn. |
| MQ7 | **Rely on agents remembering to report, or on Claude Code hooks?** | Both. Agent-written events carry meaning (story, state, message); a project hook (`PostToolUse`, throttled to one per minute, plus `Stop`) writes `heartbeat` events automatically, so liveness doesn't depend on agent discipline. The hook needs the agent name: derive it from the worktree's branch. |
| MQ8 | **Token / cost reporting?** | Spike first (T01-11). Hook input includes the session's transcript path; if usage per message can be read from the transcript, the heartbeat hook can add `tokens`/`cost_usd`. Otherwise drop it. |
| MQ9 | **Story-aware view?** | Yes, next: read the story file from the agent's branch (`git show <branch>:stories/...`) to show its title and acceptance-criteria progress (`3/5 [x]`), which is a better progress measure than self-reported `percent`. |
| MQ10 | **Liveness thresholds?** | QUIET at 5 min, STALE at 15 min, global. Agents should send a heartbeat before builds or test runs expected to take longer. Per-agent overrides only if false alarms appear. |
| MQ11 | **Agent naming?** | Agent name = branch name, enforced by the emit helper's default once hooks exist (MQ7). Explicit `branch` field otherwise. |
