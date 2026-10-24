# 06 · GitHub Issues Sync (optional)

Priority 3, and off by default (`issues.mode: "off"`). The goal is that people and tools that live in
GitHub, such as the issue list, project boards and mobile, can see and discuss the backlog. **Nothing
in this round writes to GitHub.** The planner is built in dry-run form only.

## Direction: proposal

| Option | How it works | Verdict |
|--------|--------------|---------|
| **Files to issues (one-way push)** | Story files are the truth. Issues are a mirror the tool creates and updates, and closes when a story is Done. | **Recommended** (`mode: "push"`) |
| Link-only | The tool records an issue number per story and checks that it exists, but never writes content | Supported as `mode: "link"`, for repositories where issues are curated by hand |
| Two-way | Edits on either side are merged | **Not recommended.** It needs field-level merge rules for markdown, and the spec-driven discipline says specs and stories live in git, reviewed in PRs. |

Feedback still flows back without two-way sync. In push mode, **issue comments** are read and shown as
feedback next to the story in the backlog view (AM3-03). They can optionally be forwarded to the agent
on that story as a `note` through its [inbox](03-two-way-channel.md). Editing an issue's *body* on
GitHub is the one thing push mode treats as a conflict.

## Mapping

| Story side | GitHub side |
|------------|-------------|
| Story `M00-04 · ROM loading…` | Issue titled `M00-04 · ROM loading…`. The body is the story file plus a "synced from" link and a hidden marker `<!-- agent-monitor:story=M00-04 sha=<sha1> -->`. |
| Milestone folder `M00 · Foundation` | GitHub milestone `M00 · Foundation`, created if missing |
| Status `Draft`, `Ready`, `In progress` | Label `status:draft`, `status:ready` or `status:in-progress`, issue **open** |
| Status `Done` | Label `status:done`, issue **closed** (a story moving back reopens it) |
| Every synced issue | Label `story` |
| Agent on the story (live) | Optional assignee mapping or comment (OQ-6). Not in the first version. |

Identity comes from the **marker**, not the title, so renaming a story keeps its issue. Only the
labels the tool manages (`story`, `status:*`) are changed; any other labels people add are left alone.

## Conflicts and edge cases

| Situation | Behaviour |
|-----------|-----------|
| Issue body edited on GitHub (its hash no longer matches the marker) | **Conflict**: not overwritten. Reported with the issue number; the user copies the change into the story file, then syncs again. |
| Issue closed by hand while the story isn't Done | Reopened on the next sync, with a comment explaining that the file is the truth (OQ-7) |
| Story deleted or renamed to a new ID | The old issue is reported as an **orphan**, labelled `story:orphaned`, and never closed or deleted automatically |
| Two stories with the same ID | Sync refuses to run and names both files |
| Public repository | Push mode asks for confirmation (`--yes-public`) before writing story text to a public repository |
| Rate limits | Writes are batched one by one with backoff. A full sync of ~120 stories is about 250 API calls, well inside the 5,000/hour limit. |

## Commands

- **`monitor.py issues-plan [--project P] [--offline]`** (built). This is a **dry run**. It reads
  existing issues and milestones with `gh issue list` and `gh api …/milestones` (read-only), then
  prints what a push would do: `create-milestone`, `create`, `update` (with which fields), `close`,
  `reopen`, `conflict`, `orphan`. It **never writes**: there's no apply flag in this version.
- `monitor.py issues-sync --apply` (AM3-02, not built): only after the user approves the plan for a
  repository. It needs `issues.mode: "push"` in that repository's registration file.

<!-- cspell:words ANSI ChrisC413 OQ Ollama PostToolUse Pro Subagent SubagentStop argparse backlogs cmake ctypes dedupe deduplicated fromisoformat genesisEmu gh gitignored isatty jsonl kernel32 lockfile ms msvcrt npm osascript piggy pipx rate_limits relayed relayer relayers statusLine statusline stdlib subagent subagents subtree superproject termios toolu uncached unittest unterminated wintypes worktree worktrees -->
