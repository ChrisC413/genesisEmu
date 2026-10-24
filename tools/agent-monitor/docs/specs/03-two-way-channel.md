# 03 · Two-way Channel

Priority 1 (decided as MQ6). Agents ask questions and report feedback; the user answers, steers, or
stops them. Nothing interrupts an agent mid-step: messages are picked up at **checkpoints**, moments
when the agent is already calling the helper.

## Messages

### Agent to user: questions

A question is a status event with `state: "question"`, written to the agent's own status file:

```json
{"ts":"…","agent":"m00-foundation","story":"M00-04","state":"question","qid":"q1",
 "message":"Which Sonic 2 revision should the header test pin?","options":["REV00","REV01"],
 "default":"REV01","blocking":false}
```

- `qid` is unique per agent. The helper assigns `q1`, `q2`, … when none is given, and the agent can
  pick its own (`rom-rev`).
- `default` is what the agent will do if nobody answers. A **non-blocking** question carries on with
  that default, which is the preferred style because it keeps agents moving.
- `blocking: true` means the agent can't usefully continue. The monitor flags it as `ASK`.
- The agent can resolve its own question later with `closes: "q1"` on any event.

### User to agent: inbox messages

Messages go in `<status_dir>/<agent>.inbox.jsonl`, which is append-only and has several writers:

```json
{"ts":"…","id":"m-20261024T120501-3fa2","to":"m00-foundation","from":"chris","kind":"answer","ref":"q1","text":"REV01"}
{"ts":"…","id":"m-20261024T121010-91bc","to":"m00-foundation","from":"chris","kind":"steer","text":"After M00-04 do M00-06 before M00-05"}
{"ts":"…","kind":"delivered","ref":["m-20261024T121010-91bc"],"via":"sendmessage","from":"coordinator"}
```

| Kind | Meaning | Expected agent behaviour |
|------|---------|--------------------------|
| `answer` | Answers question `ref` | Use it, and mention it in the next progress event |
| `steer` | Change direction, scope or priority | Apply it at the next checkpoint, then confirm with a progress event |
| `note` | For information only | Take it into account; no reply needed |
| `stop` | Wrap up | Finish the current step, emit a final state, and end |
| `delivered` | Bookkeeping by a relayer (not a message) | — |

### Delivery state is derived, never stored

A message counts as **delivered** in either of two cases:

- its id appears in an `ack` event in the agent's status file, or
- its id appears in a `delivered` line in the inbox.

No cursor or state files are needed, both logs are append-only, and a crash at any point at worst
delivers a message twice; it never loses one.

Question status follows from that:

| Status | Rule |
|--------|------|
| `open` | No answer yet |
| `answered` | An answer is queued but not yet delivered |
| `delivered` | The agent has the answer |
| `closed` | The agent resolved the question itself (`closes`) |

## Delivery paths

| Path | How | Latency | Cost to the agent |
|------|-----|---------|-------------------|
| **Piggy-back** (default) | Every `emit.py` call prints pending messages after writing its event, then acks them | Until the agent's next event | **None extra.** The agent already makes that call; the output is empty when there's nothing pending. |
| **Explicit check** | `emit.py check` at story boundaries or before risky steps | Agent's choice | One small tool call (≈100 tokens of call and output) |
| **Blocking wait** | `emit.py wait --qid q1 --timeout 300` after a blocking question; exits 3 on timeout | Seconds after the user answers | One tool call; **no tokens while waiting** (the process sleeps and polls the file every 2 s) |
| **Relay push** | A Claude Code session that can reach the agent with `SendMessage` (its parent coordinator, or a peer session) runs `monitor.py pending --json`, sends the text, then runs `monitor.py mark-delivered <agent> <ids> --via sendmessage` | As soon as the relayer acts | One incoming message |
| **Hook push** *(proposed, not built)* | A Claude Code hook injects pending messages as additional context | Next tool call | Hook latency per tool call ([04](04-capture-and-usage.md)) |

**Routing rule:** the inbox file is the single source of truth and the audit log, whichever way a
message travels. Every path records delivery, as an `ack` from the agent or a `delivered` line from a
relayer, so a message relayed with `SendMessage` is not printed again at the agent's next emit.

### Reconciling with Claude Code's `SendMessage`

- The monitor is not a Claude session, so it can't call `SendMessage`. Only Claude Code sessions can:
  the session that spawned a subagent, or peer sessions through cross-session messaging.
- Subagents spawned by a coordinator (how genesisEmu runs today) can be reached both ways. The
  coordinator relays `pending` messages when the user asks it to, or as part of a periodic check, and
  the agent also gets them by piggy-back if the coordinator doesn't act first.
- Agents that nothing can `SendMessage` (headless runs, scripts, Ollama agents) rely on piggy-back,
  check and wait only. This keeps the protocol agent-agnostic.
- Messages the coordinator wants to send on its own initiative can also be written to the inbox
  (`--from coordinator`), which puts them in the same audit trail.

## Trust

Inbox text reaches a Claude Code agent as **tool output**. Agents are rightly trained to treat tool
output as data rather than instructions, so a steer that arrives this way may be ignored or flagged.
Two things make the channel work without weakening that protection:

1. **Authorize it in the agent's instructions.** The task prompt the user gives each agent says that
   messages printed by `emit.py` under `[agent-monitor inbox]` come from the user and may answer
   questions or adjust scope and priorities (snippet in [07](07-repository-and-adoption.md)). That
   authority comes from the user's own prompt, not from the file.
2. **Limit what a message can ask for.** Messages may answer, re-prioritise, narrow or widen scope
   within the original task, or stop. They never grant permissions, change the agent's configuration,
   or override the permission system, and the agent should refuse such requests. Any local process can
   write an inbox file, so the inbox carries no more authority than the local user account, and less
   than the user's own messages.

## User interface

- CLI (built):
  - `monitor.py questions`
  - `monitor.py answer <agent> <qid> "text"` (checks that the question exists; `--force` sends anyway)
  - `monitor.py steer|note <agent> "text"`
  - `monitor.py stop <agent> ["text"]`
  - `monitor.py inbox <agent>`
  - `monitor.py pending [--json]`
  - `monitor.py mark-delivered <agent> <id…> [--via sendmessage]`
- The live view lists open questions with the exact command to answer them, plus inbox messages not
  yet picked up.
- In-view answering (proposed, AM1-03): press `a`, pick a question, type the answer, without leaving
  the live view.

## Agent contract (what agents are told)

- Report with `emit.py` at story start, at meaningful steps, and at the end. Each call is also a
  checkpoint.
- Ask instead of guessing on decisions the spec leaves open: a non-blocking question with a `default`
  where possible. Use blocking questions plus `wait` only when continuing would waste work.
- Read anything printed under `[agent-monitor inbox]` and act on it at the next checkpoint. Confirm
  steers in the next progress event.

<!-- cspell:words ANSI ChrisC413 OQ Ollama PostToolUse Pro Subagent SubagentStop argparse backlogs cmake ctypes dedupe deduplicated fromisoformat genesisEmu gh gitignored isatty jsonl kernel32 lockfile ms msvcrt npm osascript piggy pipx rate_limits relayed relayer relayers statusLine statusline stdlib subagent subagents subtree superproject termios toolu uncached unittest unterminated wintypes worktree worktrees -->
