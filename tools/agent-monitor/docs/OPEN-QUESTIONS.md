# Open Questions (agent-monitor)

Each question names what it blocks and a **recommended default** that the specs assume until it's
answered. Answered questions move to the resolved table and into [DECISIONS.md](DECISIONS.md).

## Open

| # | Question | Blocks | Recommended default |
|---|----------|--------|---------------------|
| OQ-1 (was MQ10) | **Liveness thresholds?** Global or per agent? | — | QUIET at 5 min, STALE at 15 min, global. Agents send a heartbeat before long builds. With transcript watching (AM4-01), liveness comes from real activity and false alarms mostly disappear. |
| OQ-2 (was MQ11) | **Agent naming?** Enforce agent name = branch name? | AM2-02 | Recommend, don't enforce. The helper warns when `-a` differs from the current branch. |
| OQ-3 | **Move to a standalone repository?** (AD-006) | AM2, AM3 | Yes: private `ChrisC413/agent-monitor`, migrated with `git subtree split` after PR #1 ([07](specs/07-repository-and-adoption.md)) |
| OQ-4 | **How is the tool found on each machine?** | AM2-04 | One clone per machine plus the `AGENT_MONITOR` environment variable. A `pip install --user -e` for `agent-emit` and `agent-monitor` commands is optional. |
| OQ-5 | **Backlog source:** `main` only, or also each agent's branch? | AM2-03 | `main`, plus the "reported done by" hint. Read branches only if the lag proves confusing. |
| OQ-6 | **Show "agent on story" in GitHub?** For example an `agent:<name>` label, or a comment when an agent starts a story. | AM3-02 | No. Keep agent activity local; it changes too often for issues. |
| OQ-7 | **Issue closed by hand while the story isn't Done:** reopen it, or leave it and warn? | AM3-02 | Warn only in the first version, and reopen once the sync has been trusted for a while |
| OQ-8 | **Should the coordinator relay inbox messages with `SendMessage` automatically?** | AM1-04 | Only when the user asks ("relay pending"), or as part of a coordinator check it's already doing. Piggy-back delivery covers the rest. |
| OQ-9 | **Status line for plan usage:** is it acceptable to add a `statusLine` command to the user's Claude Code settings? | AM4-02 | Yes, opt-in, documented. The script prints the normal status line and copies `rate_limits` to `_usage.json`. |
| OQ-10 | **Which first non-Claude runtime?** Ollama with which agent loop (for example aider, a custom script, or Open Interpreter)? | AM6-01 | Decide when the first local agent is planned. The protocol is ready; only a small runner is missing. |

## Resolved (2026-10-24, answers from Chris)

| # | Question | Answer |
|---|----------|--------|
| MQ1 | Terminal or browser UI? | Accept default: terminal now; an optional read-only localhost `--serve` later (AD-004) |
| MQ2 | Read git, PR and CI through `gh`? | Accept default: on by default, 90 s cache, `--no-gh`; failed CI counts as attention (AD-010) |
| MQ3 | Notifications? | Accept default: bell always; OS notifications with `--notify`; only blocked, failed, STALE and a finished milestone (AD-011) |
| MQ4 | Multiple machines? | Eventually, not a priority. **New requirement:** support local agents on Ollama (non-Claude), so the protocol and helpers stay agent-agnostic (AM6). |
| MQ5 | Retention? | Accept default (AD-012) |
| MQ6 | Messages and commands to agents? | **Priority:** feedback, answers and steering are built first ([03](specs/03-two-way-channel.md), AD-005, AM1) |
| MQ7 | Hooks or self-reporting? | Come back with options and a performance analysis; don't implement hooks. Done in [04](specs/04-capture-and-usage.md); the recommendation is AD-008. |
| MQ8 | Tokens and cost? | Yes, including **remaining quota on Claude Pro**. Investigate first: findings in [04](specs/04-capture-and-usage.md); the recommendation is AD-009. |
| MQ9 | Story-aware view? | Only a summary of the agent's current targeted work, with no parsing of acceptance criteria (AM2-03) |
| MQ10, MQ11 | Thresholds; naming | Still open, as OQ-1 and OQ-2 |
