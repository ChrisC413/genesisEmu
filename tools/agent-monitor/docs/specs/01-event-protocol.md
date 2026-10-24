# 01 · Event Protocol

## Location and files

- **Status directory:** `.agent-status/` at the root of the project's **main checkout**, so every
  worktree shares it. It is gitignored. Tools find it with `git rev-parse --git-common-dir`; the
  registration file's `status_dir` ([05](05-multi-project-backlog.md)) or `$AGENT_STATUS_DIR` overrides it.
- **One status file per agent:** `<agent>.jsonl`, append-only, **one writer** (the agent).
- **One inbox per agent:** `<agent>.inbox.jsonl`, append-only, several writers ([03](03-two-way-channel.md)).
- **Agent names** match `[A-Za-z0-9][A-Za-z0-9._-]{0,63}`, must not end in `.inbox`, and should equal
  the agent's branch name (OQ-2 is still open on whether to enforce this).

## v1 line format (frozen)

UTF-8 JSON Lines: one JSON object per line, terminated by `\n`.

```json
{"ts":"2026-10-24T11:22:33Z","agent":"m00-foundation","story":"M00-04","state":"progress","message":"Checksum decoded"}
```

| Field | Type | Required | Meaning |
|-------|------|----------|---------|
| `ts` | string | yes | ISO-8601 time, UTC with `Z` (readers accept offsets and treat naive times as UTC) |
| `agent` | string | yes¹ | Agent name, equal to the file stem |
| `story` | string | no | Story ID being worked on; empty between stories |
| `state` | string | yes | `started` · `progress` · `blocked` · `done` · `failed` |
| `message` | string | no | One human-readable line (readers collapse whitespace and newlines) |

¹ Readers fall back to the file stem when it is missing.

What each state means:

- `started`: began a story.
- `progress`: a meaningful step finished.
- `blocked`: can't continue without help. The message says what's needed; prefer a blocking `question` (below).
- `done`: the story's acceptance criteria are met.
- `failed`: gave up on the story.

`done` and `failed` are **terminal**, so silence after them is expected.

## Extensions (backward compatible)

All of these are optional. Readers **must ignore fields they don't recognise** and show unrecognised
states as they are, so v1 readers keep working. A breaking change would add `"v": 2`, but none is planned.

### Side-channel states

These states don't change the agent's displayed state or message.

| State | Fields | Meaning |
|-------|--------|---------|
| `heartbeat` | — | Liveness only. Hidden from the feed. Send at least every 5 minutes during long builds or tests. |
| `question` | `qid`, `options[]`, `default`, `blocking` | The agent asks the user something; `message` is the question ([03](03-two-way-channel.md)) |
| `ack` | `ref[]` (message ids) | The agent received these inbox messages. Written by the helper; hidden from the feed. |

### Optional fields on any event

| Field | Type | Meaning |
|-------|------|---------|
| `percent` | int 0–100 | Progress within the current story (resets when the story changes) |
| `branch`, `worktree` | string | Git context (added automatically by the emit helper) |
| `pr` | int | PR the agent opened |
| `closes` | string or list | Question id(s) this event resolves (the agent found the answer itself) |
| `files_changed` | int | Files changed on the branch compared with its base |
| `tests` | object | e.g. `{"passed": 12, "failed": 0}` |
| `tokens`, `cost_usd` | int, number | Usage, when the producer can get it ([04](04-capture-and-usage.md)) |
| `session_id` | string | Claude Code session or subagent id, which links the agent to its transcript |
| `host` | string | Machine name, for multi-machine use (AM6) |
| `runtime` | string | `claude-code`, `ollama:<model>`, `script`, …: what kind of agent this is (AM6) |
| `detail` | string | Longer free text, not shown in the table |

## Producing events

- **Helper (preferred):** `python <tool>/emit.py -a <agent> -s <story> <state> "<message>"`, or the
  `emit()` / `ask()` / `check_inbox()` functions. The helper:
  - checks the name and state;
  - writes `ts` in UTC;
  - collapses the message to one line (500 characters at most);
  - adds `branch` and `worktree`;
  - appends atomically;
  - **prints any pending inbox messages** ([03](03-two-way-channel.md)).
- **Any other writer:** anything that appends a correct v1 line works, for example `echo … >>`, a
  PowerShell script, or a Node or Go program driving an Ollama model. Inbox delivery then needs the
  writer to read its inbox (03, "Delivery").
- **Atomicity:** each event is a single append. On Windows the C runtime's `O_APPEND` seeks and then
  writes, and it lost lines under concurrent writers: 182 of 200 lines survived a 4-writer test. The
  helper therefore opens a `FILE_APPEND_DATA` handle on Windows and uses `O_APPEND` on POSIX. This
  matters for inboxes, which have several writers.
