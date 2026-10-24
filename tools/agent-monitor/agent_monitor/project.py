"""Projects: the per-repo registration file and the per-machine registry.

Registration file ``.agent-project.json`` at the root of each repo (committed)::

    {
      "name": "genesisEmu",
      "github": {"repo": "ChrisC413/genesisEmu", "visibility": "private"},
      "status_dir": ".agent-status",
      "stories": {"root": "stories", "milestone_glob": "*", "story_glob": "*-[0-9][0-9]-*.md"},
      "issues": {"mode": "off"}
    }

Every key is optional; defaults are shown. ``status_dir`` is relative to the *main* checkout of the git repo containing the project
(shared by all worktrees). Per-machine registry (not committed) listing the checkouts to
show together: ``$AGENT_MONITOR_HOME/projects.json`` (default ``~/.agent-monitor``)::

    {"projects": [{"path": "C:/Users/chris/Desktop/projects/genesisEmu"}]}
"""

from __future__ import annotations

import json
import os
from dataclasses import dataclass, field
from pathlib import Path

REGISTRATION_FILE = ".agent-project.json"
VISIBILITIES = ("private", "internal", "public")
ISSUE_MODES = ("off", "link", "push")


@dataclass
class Project:
    root: Path
    name: str
    repo: str = ""                 # owner/name on GitHub, "" if not linked
    visibility: str = "private"
    status_dir: Path = field(default_factory=Path)
    stories_root: Path = field(default_factory=Path)
    milestone_glob: str = "*"
    story_glob: str = "*-[0-9][0-9]-*.md"
    issues_mode: str = "off"
    registered: bool = False       # True when read from a registration file


def load_project(root: Path) -> Project:
    root = Path(root).resolve()
    data: dict = {}
    reg = root / REGISTRATION_FILE
    if reg.is_file():
        data = json.loads(reg.read_text(encoding="utf-8"))
        if not isinstance(data, dict):
            raise ValueError(f"{reg}: expected a JSON object")
    gh = data.get("github") or {}
    st = data.get("stories") or {}
    vis = gh.get("visibility", "private")
    if vis not in VISIBILITIES:
        raise ValueError(f"{reg}: github.visibility must be one of {', '.join(VISIBILITIES)}")
    mode = (data.get("issues") or {}).get("mode", "off")
    if mode not in ISSUE_MODES:
        raise ValueError(f"{reg}: issues.mode must be one of {', '.join(ISSUE_MODES)}")
    return Project(
        root=root, name=data.get("name") or root.name, repo=gh.get("repo", ""), visibility=vis,
        status_dir=_main_checkout(root) / data.get("status_dir", ".agent-status"),
        stories_root=root / st.get("root", "stories"),
        milestone_glob=st.get("milestone_glob", "*"), story_glob=st.get("story_glob", "*-[0-9][0-9]-*.md"),
        issues_mode=mode, registered=reg.is_file())


def _main_checkout(root: Path) -> Path:
    """Relative ``status_dir`` resolves against the main checkout, so worktrees share it."""
    from .gitinfo import find_repo_root
    return find_repo_root(root) or root


def registry_path() -> Path:
    home = os.environ.get("AGENT_MONITOR_HOME")
    return (Path(home) if home else Path.home() / ".agent-monitor") / "projects.json"


def registered_projects() -> list[Project]:
    try:
        data = json.loads(registry_path().read_text(encoding="utf-8"))
    except (OSError, ValueError):
        return []
    out = []
    for entry in data.get("projects", []) if isinstance(data, dict) else []:
        path = entry.get("path") if isinstance(entry, dict) else None
        if path and Path(path).is_dir():
            out.append(load_project(Path(path)))
    return out


def resolve_project(spec: str | None, cwd: Path | None = None) -> Project:
    """``spec`` is a path, a registered project name, or None (= the repo containing cwd)."""
    if spec:
        if Path(spec).is_dir():
            return load_project(Path(spec))
        for p in registered_projects():
            if p.name.lower() == spec.lower():
                return p
        raise ValueError(f"unknown project {spec!r} (not a directory or a registered name)")
    from .gitinfo import find_repo_root
    root = find_repo_root(cwd or Path.cwd()) or find_repo_root()
    if root is None:
        raise ValueError("not inside a git repository; pass --project")
    return load_project(root)
