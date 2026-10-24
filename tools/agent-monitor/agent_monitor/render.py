"""Plain-text rendering (ASCII only, so redirected output works on any Windows code page)."""

from __future__ import annotations

from datetime import datetime, timedelta
from pathlib import Path

from .gitinfo import AgentGit
from .state import LIVE_ASK, LIVE_IDLE, LIVE_OK, LIVE_QUIET, LIVE_STALE, Q_ANSWERED, Monitor

RESET = "\x1b[0m"
COLORS = {
    "started": "\x1b[36m", "progress": "\x1b[32m", "blocked": "\x1b[33;1m",
    "done": "\x1b[34;1m", "failed": "\x1b[31;1m",
    LIVE_QUIET: "\x1b[33m", LIVE_STALE: "\x1b[31;1m", "dim": "\x1b[2m", "bold": "\x1b[1m",
    "pass": "\x1b[32m", "fail": "\x1b[31;1m", "pending": "\x1b[33m",
}
LIVE_LABEL = {LIVE_OK: "", LIVE_QUIET: "QUIET", LIVE_STALE: "STALE", LIVE_IDLE: "", LIVE_ASK: "ASK"}


def fmt_age(td: timedelta | None) -> str:
    if td is None:
        return "-"
    s = int(td.total_seconds())
    if s < 60:
        return f"{s}s"
    if s < 3600:
        return f"{s // 60}m{s % 60:02d}s"
    if s < 86400:
        return f"{s // 3600}h{(s % 3600) // 60:02d}m"
    return f"{s // 86400}d{(s % 86400) // 3600:02d}h"


def _ascii(s: str) -> str:
    return s.encode("ascii", "replace").decode("ascii")


def _fit(s: str, width: int) -> str:
    s = _ascii(s)
    if width <= 0:
        return ""
    return s if len(s) <= width else s[: max(width - 3, 0)] + "..."[: width]


class Painter:
    def __init__(self, color: bool):
        self.color = color

    def __call__(self, text: str, key: str) -> str:
        code = COLORS.get(key)
        return f"{code}{text}{RESET}" if (self.color and code) else text


def render(mon: Monitor, now: datetime, *, width: int = 120, feed: int = 15,
           warn_after: timedelta, stale_after: timedelta, git: dict[str, AgentGit] | None = None,
           color: bool = False, source: str = "", show_errors: int = 3) -> list[str]:
    p = Painter(color)
    lines: list[str] = []
    local = now.astimezone()
    header = f"agent-monitor  {local:%Y-%m-%d %H:%M:%S}  {len(mon.agents)} agent(s)  {source}"
    lines.append(p(_fit(header, width), "bold"))
    lines.append("")

    cols = (("AGENT", 18), ("STORY", 8), ("STATE", 9), ("AGE", 8), ("LIVE", 6))
    fixed = sum(w + 1 for _, w in cols)
    msg_w = max(width - fixed, 10)
    lines.append(p(" ".join(f"{n:<{w}}" for n, w in cols) + " MESSAGE", "dim"))
    if not mon.agents:
        lines.append(p("  (no agents have reported yet)", "dim"))
    for a in mon.rows():
        live = a.liveness(now, warn_after, stale_after)
        pct = a.extra.get("percent")
        msg = a.message
        if isinstance(pct, (int, float)) and not isinstance(pct, bool):
            msg = f"[{int(pct):3d}%] {msg}"
        cells = [
            f"{_fit(a.name, 18):<18}",
            f"{_fit(a.story or '-', 8):<8}",
            p(f"{_fit(a.state or '-', 9):<9}", a.state),
            f"{fmt_age(a.age(now)):<8}",
            p(f"{LIVE_LABEL[live]:<6}", live),
            _fit(msg, msg_w),
        ]
        lines.append(" ".join(cells))
        g = (git or {}).get(a.name)
        if g and (g.branch or g.pr):
            # Short, important fields first; the commit subject absorbs truncation.
            head = "  " + (f"branch {g.branch}" if g.branch else "")
            if g.pr:
                head += f" | PR {g.pr}"
            line = p(_ascii(head), "dim")
            if g.checks:
                head += f" CI {g.checks}"
                line += p(" CI ", "dim") + p(g.checks, g.checks)
            if g.commit:
                line += p(_fit(" | " + g.commit, width - len(head)), "dim")
            lines.append(line)
        if a.stories_done:
            lines.append(p(_fit(f"  done: {', '.join(a.stories_done)}", width), "dim"))

    questions = mon.open_questions()
    if questions:
        lines.append("")
        lines.append(p(f"OPEN QUESTIONS ({len(questions)})", "bold") +
                     p('  answer: monitor.py answer <agent> <qid> "text"', "dim"))
        for a, q in questions:
            age = fmt_age(max(now - q.ts, timedelta(0)))
            if a.question_status(q.qid) == Q_ANSWERED:
                flag = p("answered, awaiting pickup", "dim")
            else:
                flag = p("BLOCKING", LIVE_ASK) if q.blocking else "open"
            head = f"  {_fit(a.name, 18):<18} {_fit(q.qid, 5):<5} {age:<7} "
            plain = len("answered, awaiting pickup") if a.question_status(q.qid) == Q_ANSWERED else (8 if q.blocking else 4)
            lines.append(head + flag + " " + _fit(q.text, max(width - len(head) - plain - 1, 10)))
            hints = []
            if q.options:
                hints.append("options: " + " | ".join(q.options))
            if q.default:
                hints.append("default: " + q.default)
            if hints:
                lines.append(p(_fit("      " + "   ".join(hints), width), "dim"))
    pend = [(a, len(a.pending_messages())) for a in mon.rows() if a.pending_messages()]
    if pend:
        lines.append(p(_fit("  inbox not yet picked up: " + ", ".join(f"{a.name} ({n})" for a, n in pend), width), "dim"))

    lines.append("")
    lines.append(p(f"RECENT EVENTS (last {feed})", "bold"))
    recent = mon.recent(feed)
    if not recent:
        lines.append(p("  (none)", "dim"))
    for ev in recent:
        t = ev.ts.astimezone()
        head = f"{t:%H:%M:%S} {_fit(ev.agent, 18):<18} {_fit(ev.story or '-', 8):<8} "
        st = p(f"{_fit(ev.state, 9):<9}", ev.state if ev.known_state else "")
        lines.append(head + st + " " + _fit(ev.message, max(width - len(head) - 10, 10)))

    if mon.error_count and show_errors:
        lines.append("")
        lines.append(p(f"MALFORMED LINES: {mon.error_count} (latest {min(show_errors, len(mon.errors))})", "failed"))
        for err in list(mon.errors)[-show_errors:]:
            where = f"{Path(err.source).name}:{err.lineno}" if err.source else "<input>"
            lines.append(p(_fit(f"  {where}: {err.reason}", width), "dim"))
    return lines
