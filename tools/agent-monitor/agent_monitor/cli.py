"""``monitor`` entry point: live terminal view, ``--once`` snapshot, or ``--json``."""

from __future__ import annotations

import argparse
import json
import os
import shutil
import sys
import time
from datetime import datetime, timedelta, timezone
from pathlib import Path

from .emit import default_status_dir
from .gitinfo import AgentGit, GitContext, find_repo_root
from .protocol import format_ts
from .render import render
from .state import Monitor
from .tail import DirTailer


def _enable_vt() -> bool:
    """Enable ANSI escape processing on Windows consoles. Returns False if impossible."""
    if os.name != "nt":
        return True
    try:
        import ctypes
        k32 = ctypes.windll.kernel32
        h = k32.GetStdHandle(-11)
        mode = ctypes.c_uint32()
        if not k32.GetConsoleMode(h, ctypes.byref(mode)):
            return False
        return bool(k32.SetConsoleMode(h, mode.value | 0x0004))
    except Exception:
        return False


def _is_terminal(stream) -> bool:
    """isatty() is also true for the Windows NUL device; require a real console there."""
    if not stream.isatty():
        return False
    if os.name != "nt":
        return True
    try:
        import ctypes
        import msvcrt
        mode = ctypes.c_uint32()
        handle = msvcrt.get_osfhandle(stream.fileno())
        return bool(ctypes.windll.kernel32.GetConsoleMode(handle, ctypes.byref(mode)))
    except Exception:
        return False


def snapshot_json(mon: Monitor, now: datetime, warn: timedelta, stale: timedelta,
                  git: dict[str, AgentGit], feed: int) -> dict:
    agents = []
    for a in mon.rows():
        g = git.get(a.name)
        agents.append({
            "agent": a.name, "story": a.story, "state": a.state, "message": a.message,
            "last_ts": format_ts(a.last_ts) if a.last_ts else None,
            "age_s": int(a.age(now).total_seconds()) if a.last_ts else None,
            "liveness": a.liveness(now, warn, stale), "events": a.events,
            "stories_done": a.stories_done, "extra": a.extra,
            "git": g.__dict__ if g else None,
        })
    return {
        "now": format_ts(now), "agents": agents,
        "recent": [e.to_dict() for e in mon.recent(feed)],
        "malformed_lines": mon.error_count,
    }


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(prog="monitor", description="Watch Claude Code agents live.")
    ap.add_argument("--dir", type=Path, help="status directory (default: $AGENT_STATUS_DIR or "
                    "<main checkout>/.agent-status)")
    ap.add_argument("--once", action="store_true", help="print one snapshot and exit")
    ap.add_argument("--json", action="store_true", help="print one JSON snapshot and exit")
    ap.add_argument("--interval", type=float, default=1.0, help="poll interval, seconds (default 1)")
    ap.add_argument("--warn-minutes", type=float, default=5.0, help="mark QUIET after N minutes (default 5)")
    ap.add_argument("--stale-minutes", type=float, default=15.0, help="mark STALE after N minutes (default 15)")
    ap.add_argument("--feed", type=int, default=15, help="recent events to show (default 15)")
    ap.add_argument("--no-git", action="store_true", help="skip git and gh lookups")
    ap.add_argument("--no-gh", action="store_true", help="skip gh (PR/CI) lookups")
    ap.add_argument("--color", choices=("auto", "always", "never"), default="auto")
    ap.add_argument("--width", type=int, help="output width (default: terminal width)")
    args = ap.parse_args(argv)

    status_dir = args.dir or default_status_dir()
    warn = timedelta(minutes=args.warn_minutes)
    stale = timedelta(minutes=args.stale_minutes)
    mon = Monitor()
    tailer = DirTailer(status_dir)
    repo = None if args.no_git else find_repo_root(Path.cwd()) or find_repo_root()
    ctx = GitContext(repo, use_gh=not args.no_gh) if repo else None
    git: dict[str, AgentGit] = {}

    def pump():
        for ln in tailer.poll():
            mon.ingest_line(ln.text, ln.source, ln.lineno)

    pump()
    if args.once or args.json:
        if ctx:
            for a in mon.rows():
                git[a.name] = ctx.lookup(a.name, a.extra)
        now = datetime.now(timezone.utc)
        if args.json:
            print(json.dumps(snapshot_json(mon, now, warn, stale, git, args.feed), indent=2, default=str))
        else:
            color = args.color == "always" or (args.color == "auto" and _is_terminal(sys.stdout) and
                                               not os.environ.get("NO_COLOR") and _enable_vt())
            width = args.width or min(shutil.get_terminal_size((120, 40)).columns, 160)
            print("\n".join(render(mon, now, width=width, feed=args.feed, warn_after=warn,
                                   stale_after=stale, git=git, color=color, source=str(status_dir))))
        return 0

    if not _is_terminal(sys.stdout):
        print("live mode needs a terminal; use --once or --json", file=sys.stderr)
        return 2
    vt = _enable_vt()
    color = args.color != "never" and not os.environ.get("NO_COLOR") and vt
    git_refresh_at = 0.0
    out = sys.stdout
    if vt:
        out.write("\x1b[?1049h\x1b[?25l")  # alternate screen, hide cursor
    try:
        while True:
            pump()
            if ctx and time.monotonic() - git_refresh_at > 10:
                ctx.refresh_async([(a.name, dict(a.extra)) for a in mon.rows()], git)
                git_refresh_at = time.monotonic()
            size = shutil.get_terminal_size((120, 40))
            width = args.width or size.columns
            now = datetime.now(timezone.utc)
            feed = args.feed
            while True:  # shrink the feed until the screen fits (newest events stay visible)
                lines = render(mon, now, width=width - 1, feed=feed, warn_after=warn,
                               stale_after=stale, git=git, color=color, source=str(status_dir))
                if len(lines) + 2 < size.lines or feed <= 1:
                    break
                feed -= 1
            lines += ["", "Ctrl+C to quit"]
            lines = lines[: max(size.lines - 1, 5)]
            if vt:
                out.write("\x1b[H" + "\n".join(l + "\x1b[K" for l in lines) + "\x1b[J")
            else:
                os.system("cls" if os.name == "nt" else "clear")
                out.write("\n".join(lines))
            out.flush()
            time.sleep(args.interval)
    except KeyboardInterrupt:
        pass
    finally:
        if vt:
            out.write("\x1b[?25h\x1b[?1049l")
            out.flush()
    return 0


if __name__ == "__main__":
    sys.exit(main())
