"""Stories -> GitHub Issues sync: PLANNING ONLY (docs/specs/06-github-issues-sync.md).

This module computes what a one-way sync (story files are the source of truth) *would*
do and prints it. It never creates, edits or closes issues or milestones; the only
GitHub calls are read-only ``gh issue list`` / ``gh api .../milestones``.

Identity: each synced issue carries a hidden marker in its body::

    <!-- agent-monitor:story=M00-04 sha=<sha1 of the body we wrote> -->

so renamed titles don't break the link, and an issue body edited on GitHub (its content
no longer matches ``sha``) is reported as a conflict instead of being overwritten.
"""

from __future__ import annotations

import hashlib
import json
import re
from dataclasses import dataclass
from pathlib import Path

from .backlog import Milestone, Story, load_backlog
from .project import Project, resolve_project

MARKER_RE = re.compile(r"<!--\s*agent-monitor:story=(?P<id>[A-Za-z0-9-]+)\s+sha=(?P<sha>[0-9a-f]{40})\s*-->")
STORY_LABEL = "story"
STATUS_LABEL_PREFIX = "status:"


def status_label(status: str) -> str:
    return STATUS_LABEL_PREFIX + status.lower().replace(" ", "-")


def milestone_title(m: Milestone) -> str:
    return f"{m.id} · {m.title}"


def issue_title(s: Story) -> str:
    return f"{s.id} · {s.title}"


def _norm(text: str) -> str:
    return "\n".join(line.rstrip() for line in text.replace("\r\n", "\n").strip().splitlines())


def body_sha(body_without_marker: str) -> str:
    return hashlib.sha1(_norm(body_without_marker).encode("utf-8")).hexdigest()


def render_body(story: Story, project: Project, story_text: str) -> str:
    rel = str(story.path.relative_to(project.root)).replace("\\", "/")
    link = f"https://github.com/{project.repo}/blob/main/{rel}" if project.repo else rel
    content = (f"_Synced from [`{rel}`]({link}). Edit the file, not this issue: the file is the source of "
               f"truth._\n\n{story_text.strip()}")
    return f"{content}\n\n<!-- agent-monitor:story={story.id} sha={body_sha(content)} -->"


def split_marker(body: str) -> tuple[str, str | None, str | None]:
    m = MARKER_RE.search(body or "")
    if not m:
        return body or "", None, None
    return (body[: m.start()] + body[m.end():]).strip(), m["id"], m["sha"]


@dataclass
class Action:
    kind: str     # create-milestone | create | update | close | reopen | conflict | orphan | ok
    target: str   # story id / milestone title / issue number
    detail: str = ""

    def __str__(self) -> str:
        return f"{self.kind:<16} {self.target:<10} {self.detail}".rstrip()


def plan(project: Project, milestones: list[Milestone], issues: list[dict], gh_milestones: list[str],
         read_text=lambda p: Path(p).read_text(encoding="utf-8", errors="replace")) -> list[Action]:
    actions: list[Action] = []
    have_ms = set(gh_milestones)
    for m in milestones:
        if milestone_title(m) not in have_ms:
            actions.append(Action("create-milestone", m.id, f'"{milestone_title(m)}"'))
    by_story: dict[str, dict] = {}
    for iss in issues:
        _, sid, _ = split_marker(iss.get("body", ""))
        if sid:
            by_story[sid] = iss
    seen = set()
    for m in milestones:
        for s in m.stories:
            seen.add(s.id)
            want_body = render_body(s, project, read_text(s.path))
            want_labels = {STORY_LABEL, status_label(s.status)}
            want_state = "CLOSED" if s.status == "Done" else "OPEN"
            iss = by_story.get(s.id)
            if iss is None:
                actions.append(Action("create", s.id, f'"{issue_title(s)}" labels={sorted(want_labels)} '
                                                      f'milestone="{milestone_title(m)}"'
                                                      + (" then close (Done)" if want_state == "CLOSED" else "")))
                continue
            num = f"#{iss.get('number')}"
            content, _, sha = split_marker(iss.get("body", ""))
            if sha and body_sha(content) != sha:
                actions.append(Action("conflict", s.id, f"{num} body was edited on GitHub; not overwriting "
                                                        "(copy the change into the story file, then re-sync)"))
                continue
            changes = []
            if iss.get("title") != issue_title(s):
                changes.append("title")
            if split_marker(want_body)[2] != sha:
                changes.append("body")
            labels = {lab.get("name") if isinstance(lab, dict) else str(lab) for lab in iss.get("labels") or []}
            managed = {lab for lab in labels if lab == STORY_LABEL or lab.startswith(STATUS_LABEL_PREFIX)}
            if managed != want_labels:
                changes.append(f"labels {sorted(managed)} -> {sorted(want_labels)}")
            ms = iss.get("milestone") or {}
            if (ms.get("title") if isinstance(ms, dict) else ms) != milestone_title(m):
                changes.append("milestone")
            if changes:
                actions.append(Action("update", s.id, f"{num}: " + ", ".join(changes)))
            state = str(iss.get("state", "OPEN")).upper()
            if state != want_state:
                actions.append(Action("close" if want_state == "CLOSED" else "reopen", s.id, num))
            if not changes and state == want_state:
                actions.append(Action("ok", s.id, num))
    for sid, iss in by_story.items():
        if sid not in seen:
            actions.append(Action("orphan", f"#{iss.get('number')}", f"story {sid} no longer exists; "
                                                                     "would be labelled, not closed"))
    return actions


def fetch_issues(project: Project) -> tuple[list[dict], list[str]]:
    """Read-only GitHub queries via gh. Raises RuntimeError if gh is unavailable."""
    from .gitinfo import _run, find_gh
    gh = find_gh()
    if not gh:
        raise RuntimeError("gh not found; use --offline")
    out = _run([gh, "issue", "list", "--repo", project.repo, "--state", "all", "--label", STORY_LABEL,
                "--limit", "1000", "--json", "number,title,body,state,labels,milestone"], timeout=60)
    if out is None:
        # The label may not exist yet, which gh reports as an error: treat as "no issues".
        out = "[]"
    ms = _run([gh, "api", f"repos/{project.repo}/milestones?state=all&per_page=100"], timeout=60) or "[]"
    return json.loads(out), [m.get("title", "") for m in json.loads(ms)]


def main_issues_plan(args) -> int:
    project = resolve_project(args.project)
    print("DRY RUN - nothing will be created, edited or closed on GitHub.")
    if not project.repo:
        print(f"project {project.name} has no github.repo in .agent-project.json; planning against an empty repo")
    if project.issues_mode == "off":
        print(f"note: issues.mode is 'off' for {project.name}; this is only a preview")
    issues, gh_ms = [], []
    if project.repo and not args.offline:
        try:
            issues, gh_ms = fetch_issues(project)
        except (RuntimeError, ValueError) as exc:
            print(f"could not read GitHub ({exc}); planning as if the repo had no issues")
    milestones = load_backlog(project)
    actions = plan(project, milestones, issues, gh_ms)
    counts: dict[str, int] = {}
    for a in actions:
        counts[a.kind] = counts.get(a.kind, 0) + 1
        if a.kind != "ok":
            print(" ", a)
    print("summary: " + (", ".join(f"{k} {v}" for k, v in sorted(counts.items())) or "nothing to do"))
    return 0
