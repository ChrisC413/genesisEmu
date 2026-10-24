# 04 · Capture and Usage

Two decisions the user asked for evidence on before anything is built:

- **MQ7:** how heartbeats and progress get captured without hurting agent performance.
- **MQ8:** what usage and quota data is realistically available on the **Claude Pro** plan.

The measurements below were taken on the development machine (Windows 11, Python 3.13, Claude Code
2.1.309/310) on 2026-10-24.

## Measurements

| What | Result |
|------|--------|
| `python -c pass` process start | median 30 ms |
| `emit.py heartbeat --no-git` | median 132 ms (Python start plus imports) |
| `emit.py heartbeat` with git context | median 213 ms (two extra `git rev-parse` calls) |
| M00 agent transcript after 43 min | 2.8 MB, 1,089 records, 221 tool calls, 146 API requests |
| Same agent's tokens (deduplicated by request) | 157 k output, 24.7 M cache-read, 543 k cache-creation, 292 uncached input |

## Capture options (MQ7)

| Option | Latency per tool call | Tokens and context | What it captures | Failure modes |
|--------|-----------------------|--------------------|------------------|---------------|
| **A. Self-report only** (today) | 0 | Each `emit.py` call is a tool call, about 60–150 tokens plus one model round trip (seconds), unless chained onto a command the agent runs anyway (`… && emit.py …`) | Meaning: story, state, message, questions | The agent forgets; it goes silent during long builds; it can die without a final `failed` |
| **B1. Async `PostToolUse` hook** writing a throttled heartbeat (≤1 per minute) | ≈0 (`"async": true` doesn't block the tool call; the process still costs about 30–130 ms of CPU) | 0 (async output doesn't enter the context) | Liveness; tool name and arguments, so "running cmake --build" | Applies to **every** session in the project, including the coordinator. A broken script fails on every call. Whether hooks fire for subagent tool calls must be verified. The agent name must come from the worktree branch. |
| **B2. Sync `PostToolUse` hook** injecting pending inbox messages as additional context | +30 ms (a minimal Python script) to +300 ms or more (PowerShell start) on **every** tool call, inbox or not | Only when a message is delivered | Push delivery within one tool call | Latency on every call. A hook timeout or crash shows up as tool errors. Same project-wide scope as B1. |
| **B3. `Stop` / `SubagentStop` hook** | 0 at run time (fires once at the end) | 0 | Reliable final event, even when the agent forgets | Doesn't know whether the story succeeded; only knows that the agent stopped |
| **C. Wrapper / launcher** (`claude -p --output-format stream-json`, or a loop around an Ollama agent) | 0 | 0 | Everything the runtime streams: tool calls, results, usage | Only covers agents **launched by the wrapper**, not subagents spawned inside a VS Code session (how genesisEmu runs today). A wrapper crash blinds the monitor. |
| **D. Transcript watching**: the monitor tails `~/.claude/projects/<project>/<session>/subagents/agent-<id>.jsonl`, and `agent-<id>.meta.json` gives `worktreePath`, `description` and `agentType` | **0**: read-only, outside the agent | **0** | Exact liveness (last write), every tool call, last assistant text, **per-request token usage**, model, errors | Internal, **undocumented format** that may change with any Claude Code release, so it needs a defensive parser and a version check. Files are large (2.8 MB in 43 min), so tail incrementally. Contains everything the agent saw, so keep it local and display only summaries. Claude Code only. |
| **E. Status line script** (main interactive session) | 0 for agents | 0 | Account-wide plan usage (see MQ8) and the main session's context window | Runs only where the status line runs; it's unclear whether the VS Code extension runs `statusLine` commands, to be checked. Doesn't cover subagents. |

### Recommendation

1. **D + A now.** Transcript watching gives liveness, activity and token counts for Claude Code agents at
   zero cost to them. Self-reports stay the source of *meaning* (story, state, questions). Map agents to
   transcripts through `meta.json` `worktreePath`, matched against the worktree of the agent's branch
   (AM4-01).
2. **E for plan usage** (AM4-02), opt-in, since it means adding a `statusLine` command to the user's settings.
3. **No hooks yet.** If live push delivery turns out to matter, try **B1-style async** delivery first:
   an async hook can't inject context, so measure B2 with a minimal script (target under 50 ms) before
   adopting it. Use B3 only if transcripts prove unreliable for detecting the end of a run.
4. **C for non-Claude agents** (Ollama, AM6): a small runner that wraps the agent loop and emits events
   is the natural fit there.

## Usage and quota on Claude Pro (MQ8)

| Data | Available? | Source and notes |
|------|------------|------------------|
| Tokens per agent (input, output, cache read and creation, thinking), model, per request | **Yes, exact** | `message.usage` on assistant records in each transcript. Deduplicate by `requestId`, because streamed messages repeat the usage. |
| Session totals, lines added and removed, API time | **Yes** | `cost-state` records in the parent session transcript (e.g. `totalLinesAdded`, `modelUsage`) |
| "Cost" in dollars | **Notional only** | `cost-state.totalCostUSD` is the API-equivalent price. On Pro nothing is billed per token, so show it as relative size, never as spend. |
| **Plan quota: 5-hour and 7-day windows** | **Yes, as percentages** | The Claude Code status line input has a `rate_limits` field for claude.ai subscribers: per-window `used_percentage` and `resets_at` (`/usage` also shows model-specific weekly rows; whether the status line has them is unverified). A status line script can copy it to `<status_dir>/_usage.json`. Exact key names to be confirmed in AM4-02. |
| Usage-limit hits | **Probably** | Background-agent notifications say when an agent was stopped by a usage limit; the transcript form needs checking (AM4-01) |
| `/usage` dialog | Human-readable only | Interactive; not scriptable |
| Remaining quota as an absolute number of tokens or messages | **No** | Pro limits are exposed as percentages of a window, not budgets |
| Per-agent share of the quota | **No** (estimate only) | Not reported. It could be approximated by apportioning the window's percentage by each agent's token counts; label it as an estimate. |
| The plan-usage endpoint behind `/usage` | **Don't use** | Undocumented, needs Claude Code's stored login token, fragile, and possibly outside the terms of use |

**Proposed display:** a header line such as `plan: 5h 42% (resets 14:10) · 7d 18% (resets Thu)`, from
`_usage.json` with its age shown. Per-agent `tok` columns show output tokens and cache-read tokens
from transcripts, with notional cost as a tooltip-style suffix in `--json` only.
