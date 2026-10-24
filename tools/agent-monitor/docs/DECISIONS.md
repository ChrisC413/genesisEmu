# Decision Log (agent-monitor)

Short-form ADRs, the same convention as genesisEmu. To change a decision, add a new entry that
supersedes the old one; don't rewrite history. genesisEmu's D-023 records the tool's original technology
choice from the genesisEmu side.

| ID | Date | Decision | Rationale | Status |
|----|------|----------|-----------|--------|
| AD-001 | 2026-10-24 | **Python 3.9+ standard library only** | Already present on Windows; macOS 13's Command Line Tools ship 3.9. The standard library covers JSON, subprocess, threading, unittest, and ctypes for the Windows console and atomic appends, so there's nothing to install and nothing to lock. Node would need npm packages for a comparable TUI. | Proposed (= genesisEmu D-023) |
| AD-002 | 2026-10-24 | **Poll files once a second; no OS file events** | Identical on Windows and macOS, no dependencies, and negligible cost for a handful of small files | Accepted |
| AD-003 | 2026-10-24 | **Protocol v1 is frozen; extensions are additive and optional** | Agents already in flight keep working; any runtime can produce events | Accepted |
| AD-004 | 2026-10-24 | **Terminal UI first; an optional read-only `--serve` on localhost later** (MQ1) | Answer from the user | Accepted |
| AD-005 | 2026-10-24 | **Two-way channel through per-agent inbox files; delivery state derived from acks and relay records** (MQ6) | The user made two-way communication a priority. Files keep it agent-agnostic. Piggy-back delivery costs agents nothing extra. Derived state survives crashes and avoids double delivery when a coordinator relays through `SendMessage`. | Accepted |
| AD-006 | 2026-10-24 | **Move the tool to its own private repository `ChrisC413/agent-monitor`** | Multi-project scope; a stable path for agents; its own specs, stories and CI ([07](specs/07-repository-and-adoption.md)) | Proposed |
| AD-007 | 2026-10-24 | **Stories to Issues sync is one-way (files are the truth), opt-in per project, identified by a hidden marker; edits made on GitHub are reported as conflicts, never overwritten** | Keeps the spec-driven discipline (stories are reviewed in PRs) while letting GitHub show the backlog ([06](specs/06-github-issues-sync.md)) | Proposed |
| AD-008 | 2026-10-24 | **Capture: transcript watching plus self-reports; no Claude Code hooks for now** (MQ7) | Zero latency and zero tokens for agents. Hooks add per-call cost and apply project-wide ([04](specs/04-capture-and-usage.md)). | Proposed |
| AD-009 | 2026-10-24 | **Plan usage from the status line `rate_limits` field only; never call undocumented endpoints** (MQ8) | Pro exposes usage as window percentages; the `/usage` endpoint is undocumented and needs stored credentials | Proposed |
| AD-010 | 2026-10-24 | **`gh` lookups on by default, cached 90 s; failed CI counts as an attention signal** (MQ2) | Answer from the user | Accepted |
| AD-011 | 2026-10-24 | **Notifications: terminal bell always; OS notifications with `--notify`; only for blocked, failed, STALE, a new blocking question, and a finished milestone** (MQ3) | Answer from the user; blocking questions added because of AD-005 | Accepted |
| AD-012 | 2026-10-24 | **Retention: keep everything, hide agents idle for more than 7 days, archive the files of agents whose worktree is gone** (MQ5) | Answer from the user | Accepted |

<!-- cspell:words ANSI ChrisC413 OQ Ollama PostToolUse Pro Subagent SubagentStop argparse backlogs cmake ctypes dedupe deduplicated fromisoformat genesisEmu gh gitignored isatty jsonl kernel32 lockfile ms msvcrt npm osascript piggy pipx rate_limits relayed relayer relayers statusLine statusline stdlib subagent subagents subtree superproject termios toolu uncached unittest unterminated wintypes worktree worktrees -->
