# T01 · Agent monitoring

**Goal:** Watch parallel Claude Code agents live — story, state, messages, progress, problems — without interrupting them. Development tooling; independent of the emulator milestones.

**Depends on:** — (T01-12 waits for M00-02)

**Spec:** [17-agent-monitoring](../../docs/specs/17-agent-monitoring.md) · **Tool:** [tools/agent-monitor](../../tools/agent-monitor/README.md) · **Decision:** D-023

## Testable stage (exit criteria)

`python tools/agent-monitor/monitor.py` on Windows 11 and macOS 13+ shows every agent writing to `.agent-status/` with its story, state, age, liveness, branch/PR/CI, and a recent-events feed; `python -m unittest discover -s tools/agent-monitor/tests` passes. (MVP: T01-01..03.)

## Stories

| ID | Story | Depends on | Status |
|----|-------|------------|--------|
| T01-01 | [Event protocol v1 and emit helper](T01-01-event-protocol-and-emit.md) | — | Done |
| T01-02 | [Live terminal monitor MVP](T01-02-terminal-monitor-mvp.md) | T01-01 | Done |
| T01-03 | [Branch, commit, PR and CI status per agent](T01-03-git-pr-ci-enrichment.md) | T01-02 | Done |
| T01-04 | [Automatic heartbeats from Claude Code hooks](T01-04-claude-code-heartbeat-hooks.md) | T01-01 | Draft |
| T01-05 | [Attention signals and notifications](T01-05-attention-and-notifications.md) | T01-02, T01-03 | Draft |
| T01-06 | [Story-aware view (title and acceptance progress)](T01-06-story-aware-view.md) | T01-03 | Draft |
| T01-07 | [Optional local web view](T01-07-local-web-view.md) | T01-02 | Draft |
| T01-08 | [Retention, filtering and archiving](T01-08-retention-and-archiving.md) | T01-02 | Draft |
| T01-09 | [Cooperative agent inbox (messages to agents)](T01-09-agent-inbox.md) | T01-04 | Draft |
| T01-10 | [Multi-machine aggregation](T01-10-multi-machine.md) | T01-02 | Draft |
| T01-11 | [Spike: token and cost reporting](T01-11-token-cost-spike.md) | T01-04 | Draft |
| T01-12 | [Run agent-monitor tests in CI](T01-12-tools-ci.md) | T01-02, M00-02 | Draft |
