"""Reads a project's story files (milestone folders, one file per story) and joins them
with agent activity: which agent is on which story (docs/specs/05-multi-project-backlog.md).

Story file conventions (same as genesisEmu ``stories/_TEMPLATE.md``)::

    # M00-04 · ROM loading and header parsing
    **Status:** Draft | Ready | In progress | Done
    **Depends on:** M00-01, M00-03
"""

from __future__ import annotations

import json
import re
from dataclasses import dataclass, field
from pathlib import Path

from .project import Project, registered_projects, resolve_project
from .protocol import TERMINAL_STATES
from .state import Monitor

STATUSES = ("Draft", "Ready", "In progress", "Done")
TITLE_RE = re.compile(r"^#\s+(?P<id>[A-Za-z]+[0-9]*-[0-9]+)\s*(?:·|-|:|–|—)\s*(?P<title>.+?)\s*$")
MILESTONE_TITLE_RE = re.compile(r"^#\s+(?P<id>[A-Za-z]+[0-9]*)\s*(?:·|-|:|–|—)\s*(?P<title>.+?)\s*$")
FIELD_RE = re.compile(r"^\*\*(?P<key>[A-Za-z ]+):\*\*\s*(?P<value>.*?)\s*$")


@dataclass
class Story:
    id: str
    title: str
    status: str
    path: Path
    milestone: str
    depends: list[str] = field(default_factory=list)
    agents: list[str] = field(default_factory=list)       # agents currently on it
    reported_done_by: list[str] = field(default_factory=list)  # agent said done; file may lag


@dataclass
class Milestone:
    folder: str
    id: str
    title: str
    stories: list[Story] = field(default_factory=list)

    def counts(self) -> dict[str, int]:
        c = {s: 0 for s in STATUSES}
        for st in self.stories:
            c[st.status if st.status in c else "Draft"] += 1
        return c


def parse_story(path: Path, milestone: str) -> Story | None:
    try:
        text = path.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return None
    sid, title, status, depends = "", "", "Draft", []
    for line in text.splitlines()[:20]:
        if not sid:
            m = TITLE_RE.match(line)
            if m:
                sid, title = m["id"], m["title"]
                continue
        m = FIELD_RE.match(line)
        if m:
            key, value = m["key"].strip().lower(), m["value"]
            if key == "status":
                status = next((s for s in STATUSES if value.lower().startswith(s.lower())), value or "Draft")
            elif key == "depends on":
                depends = [d.strip() for d in re.split(r"[,;]", value) if re.match(r"^[A-Za-z]+[0-9]*-[0-9]+$", d.strip())]
    if not sid:
        return None
    return Story(id=sid, title=title, status=status, path=path, milestone=milestone, depends=depends)


def load_backlog(project: Project) -> list[Milestone]:
    out: list[Milestone] = []
    root = project.stories_root
    if not root.is_dir():
        return out
    for folder in sorted(p for p in root.glob(project.milestone_glob) if p.is_dir()):
        mid, mtitle = folder.name, folder.name
        readme = folder / "README.md"
        if readme.is_file():
            first = readme.read_text(encoding="utf-8", errors="replace").splitlines()[:1]
            m = MILESTONE_TITLE_RE.match(first[0]) if first else None
            if m:
                mid, mtitle = m["id"], m["title"]
        ms = Milestone(folder=folder.name, id=mid, title=mtitle)
        for f in sorted(folder.glob(project.story_glob)):
            st = parse_story(f, mid)
            if st:
                ms.stories.append(st)
        if ms.stories:
            out.append(ms)
    return out


def attach_agents(milestones: list[Milestone], mon: Monitor) -> None:
    by_id = {s.id: s for m in milestones for s in m.stories}
    for a in mon.rows():
        if a.story in by_id and a.state not in TERMINAL_STATES:
            by_id[a.story].agents.append(a.name)
        for sid in a.stories_done:
            if sid in by_id and by_id[sid].status != "Done":
                by_id[sid].reported_done_by.append(a.name)


def render_backlog(project: Project, milestones: list[Milestone], only: str | None = None) -> list[str]:
    vis = f", {project.visibility}" if project.repo else ""
    lines = [f"PROJECT {project.name}  ({project.repo or 'no GitHub repo linked'}{vis})  {project.root}"]
    if not milestones:
        lines.append("  (no stories found under " + str(project.stories_root) + ")")
    for m in milestones:
        if only and not m.folder.lower().startswith(only.lower()) and m.id.lower() != only.lower():
            continue
        c = m.counts()
        summary = "  ".join(f"{k.lower()} {v}" for k, v in c.items() if v)
        lines.append(f"  {m.id:<6} {m.title[:40]:<40} {len(m.stories):>3} stories   {summary}")
        for s in m.stories:
            if s.status == "Done" and not s.agents and not only:
                continue  # keep the overview short: finished stories are counted, not listed
            who = ""
            if s.agents:
                who = "  <- " + ", ".join(s.agents)
            if s.reported_done_by:
                who += "  (reported done by " + ", ".join(s.reported_done_by) + ")"
            lines.append(f"      {s.id:<8} {s.status:<12} {s.title[:60]}{who}")
    return [ln.encode("ascii", "replace").decode("ascii") for ln in lines]


def backlog_json(project: Project, milestones: list[Milestone]) -> dict:
    return {
        "project": project.name, "repo": project.repo, "visibility": project.visibility,
        "milestones": [{
            "id": m.id, "folder": m.folder, "title": m.title, "counts": m.counts(),
            "stories": [{"id": s.id, "title": s.title, "status": s.status, "depends": s.depends,
                         "agents": s.agents, "reported_done_by": s.reported_done_by,
                         "path": str(s.path.relative_to(project.root)).replace("\\", "/")}
                        for s in m.stories],
        } for m in milestones],
    }


def main_backlog(args, mon_default: Monitor) -> int:
    from .commands import load_monitor
    projects = registered_projects() if args.all else [resolve_project(args.project)]
    if args.all and not projects:
        print("no registered projects (see docs/specs/05-multi-project-backlog.md, projects.json)")
        return 1
    docs, text = [], []
    for proj in projects:
        mon = mon_default if (not args.all and not args.dir and not args.project) else load_monitor(
            Path(args.dir) if args.dir else proj.status_dir)
        ms = load_backlog(proj)
        attach_agents(ms, mon)
        docs.append(backlog_json(proj, ms))
        text.extend(render_backlog(proj, ms, args.milestone) + [""])
    if args.json:
        print(json.dumps(docs if args.all else docs[0], indent=2))
    else:
        print("\n".join(text).rstrip())
    return 0
