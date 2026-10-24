"""Optional git / GitHub context per agent. Every call degrades to ``None`` on failure.

Branch resolution for an agent, first match wins:
  1. the ``branch`` extension field of its latest events,
  2. a worktree whose ``worktree`` path matches the event's ``worktree`` field,
  3. a local worktree checked out on a branch named like the agent (``m00-foundation``
     or ``*/m00-foundation``).
"""

from __future__ import annotations

import json
import os
import shutil
import subprocess
import sys
import threading
import time
from dataclasses import dataclass
from pathlib import Path

GIT_TTL = 15.0
GH_TTL = 90.0
TIMEOUT = 20.0


def _run(args: list[str], cwd: str | Path | None = None, timeout: float = TIMEOUT) -> str | None:
    try:
        kw = {}
        if sys.platform == "win32":
            kw["creationflags"] = getattr(subprocess, "CREATE_NO_WINDOW", 0)
        p = subprocess.run(args, cwd=cwd, capture_output=True, text=True, encoding="utf-8",
                           errors="replace", timeout=timeout, **kw)
    except (OSError, subprocess.SubprocessError):
        return None
    return p.stdout if p.returncode == 0 else None


def find_gh() -> str | None:
    env = os.environ.get("AGENT_MONITOR_GH")
    if env:
        return env if Path(env).exists() else None
    found = shutil.which("gh")
    if found:
        return found
    for cand in (r"C:\Program Files\GitHub CLI\gh.exe", "/opt/homebrew/bin/gh", "/usr/local/bin/gh"):
        if Path(cand).exists():
            return cand
    return None


def find_repo_root(start: Path | None = None) -> Path | None:
    """Main checkout root (not the worktree), via ``git rev-parse --git-common-dir``."""
    start = Path(start or Path(__file__).resolve().parent)
    out = _run(["git", "rev-parse", "--path-format=absolute", "--git-common-dir"], cwd=start)
    if not out:
        return None
    common = Path(out.strip())
    return common.parent if common.name == ".git" else None


@dataclass
class Worktree:
    path: str
    head: str = ""
    branch: str = ""   # short name, "" when detached


def parse_worktree_porcelain(text: str) -> list[Worktree]:
    trees: list[Worktree] = []
    cur: Worktree | None = None
    for line in text.splitlines():
        if line.startswith("worktree "):
            cur = Worktree(path=line[len("worktree "):].strip())
            trees.append(cur)
        elif cur is not None and line.startswith("HEAD "):
            cur.head = line[5:].strip()
        elif cur is not None and line.startswith("branch "):
            ref = line[7:].strip()
            cur.branch = ref[len("refs/heads/"):] if ref.startswith("refs/heads/") else ref
    return trees


def summarize_checks(rollup: list | None) -> str:
    """Collapse gh's ``statusCheckRollup`` into pass / fail / pending / none."""
    if not rollup:
        return "none"
    failed = pending = 0
    for c in rollup:
        if not isinstance(c, dict):
            continue
        if c.get("__typename") == "StatusContext" or ("state" in c and "status" not in c):
            s = (c.get("state") or "").upper()
            if s in ("FAILURE", "ERROR"):
                failed += 1
            elif s in ("PENDING", "EXPECTED", ""):
                pending += 1
        else:
            status = (c.get("status") or "").upper()
            concl = (c.get("conclusion") or "").upper()
            if status != "COMPLETED":
                pending += 1
            elif concl in ("FAILURE", "TIMED_OUT", "CANCELLED", "ACTION_REQUIRED", "STARTUP_FAILURE"):
                failed += 1
    if failed:
        return "fail"
    if pending:
        return "pending"
    return "pass"


@dataclass
class AgentGit:
    branch: str = ""
    commit: str = ""          # "<sha> <subject> (<relative time>)"
    pr: str = ""              # "#12 draft OPEN" etc.
    checks: str = ""          # pass/fail/pending/none
    pr_url: str = ""


class GitContext:
    """Caches git/gh lookups. ``refresh_async`` runs them off the UI thread."""

    def __init__(self, repo_root: Path | None, use_gh: bool = True):
        self.repo_root = repo_root
        self.gh = find_gh() if use_gh else None
        self._worktrees: list[Worktree] = []
        self._wt_at = 0.0
        self._commit: dict[str, tuple[float, str]] = {}
        self._pr: dict[str, tuple[float, dict | None]] = {}
        self._lock = threading.Lock()
        self._busy = False

    # --- lookups -------------------------------------------------------------------
    def worktrees(self) -> list[Worktree]:
        if self.repo_root and time.monotonic() - self._wt_at > GIT_TTL:
            out = _run(["git", "worktree", "list", "--porcelain"], cwd=self.repo_root)
            if out is not None:
                self._worktrees = parse_worktree_porcelain(out)
            self._wt_at = time.monotonic()
        return self._worktrees

    def branch_for(self, agent: str, extra: dict) -> str:
        if isinstance(extra.get("branch"), str) and extra["branch"]:
            return extra["branch"]
        trees = self.worktrees()
        wt = extra.get("worktree")
        if isinstance(wt, str) and wt:
            norm = os.path.normcase(os.path.normpath(wt))
            for t in trees:
                if os.path.normcase(os.path.normpath(t.path)) == norm and t.branch:
                    return t.branch
        for t in trees:
            if t.branch == agent or t.branch.endswith("/" + agent):
                return t.branch
        return ""

    def commit_for(self, branch: str) -> str:
        hit = self._commit.get(branch)
        if hit and time.monotonic() - hit[0] < GIT_TTL:
            return hit[1]
        out = _run(["git", "log", "-1", "--format=%h %s (%cr)", branch, "--"], cwd=self.repo_root) or ""
        self._commit[branch] = (time.monotonic(), out.strip())
        return out.strip()

    def pr_for(self, branch: str) -> dict | None:
        if not self.gh or not self.repo_root:
            return None
        hit = self._pr.get(branch)
        if hit and time.monotonic() - hit[0] < GH_TTL:
            return hit[1]
        out = _run([self.gh, "pr", "list", "--head", branch, "--state", "all", "--limit", "1",
                    "--json", "number,state,isDraft,url,statusCheckRollup"], cwd=self.repo_root)
        pr = None
        if out:
            try:
                items = json.loads(out)
                pr = items[0] if items else None
            except (ValueError, IndexError):
                pr = None
        self._pr[branch] = (time.monotonic(), pr)
        return pr

    def lookup(self, agent: str, extra: dict) -> AgentGit:
        info = AgentGit()
        if not self.repo_root:
            return info
        info.branch = self.branch_for(agent, extra)
        if not info.branch:
            return info
        info.commit = self.commit_for(info.branch)
        pr = self.pr_for(info.branch)
        if pr:
            draft = " draft" if pr.get("isDraft") else ""
            info.pr = f"#{pr.get('number')}{draft} {str(pr.get('state', '')).lower()}"
            info.checks = summarize_checks(pr.get("statusCheckRollup"))
            info.pr_url = pr.get("url") or ""
        return info

    # --- background refresh ----------------------------------------------------------
    def refresh_async(self, agents: list[tuple[str, dict]], sink: dict[str, AgentGit]) -> None:
        with self._lock:
            if self._busy:
                return
            self._busy = True

        def work():
            try:
                for name, extra in agents:
                    sink[name] = self.lookup(name, extra)
            finally:
                with self._lock:
                    self._busy = False

        threading.Thread(target=work, daemon=True).start()
