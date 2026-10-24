"""Append one status event correctly (docs/specs/01-event-protocol.md, "Producing events").

Library use::

    from agent_monitor.emit import emit
    emit("m00-foundation", "M00-04", "progress", "Header parser passes 12/12 tests", percent=60)

CLI use (see ``python tools/agent-monitor/emit.py --help``)::

    python tools/agent-monitor/emit.py -a m00-foundation -s M00-04 progress "Header parser done"

Guarantees: one complete ``\\n``-terminated UTF-8 line written with a single atomic append
(``O_APPEND`` on POSIX, a ``FILE_APPEND_DATA`` handle on Windows), so concurrent writers never
lose or interleave lines; the message collapsed to one line; ``ts`` in UTC with a ``Z`` suffix.
"""

from __future__ import annotations

import argparse
import json
import os
import re
import sys
from datetime import datetime, timezone
from pathlib import Path

from .protocol import KNOWN_STATES, format_ts

AGENT_RE = re.compile(r"^[A-Za-z0-9][A-Za-z0-9._-]{0,63}$")
MAX_MESSAGE = 500


def default_status_dir() -> Path:
    """``$AGENT_STATUS_DIR``, else ``<main checkout>/.agent-status`` (works from any worktree)."""
    env = os.environ.get("AGENT_STATUS_DIR")
    if env:
        return Path(env)
    from .gitinfo import find_repo_root
    for start in (Path.cwd(), Path(__file__).resolve().parent):
        root = find_repo_root(start)
        if root:
            return root / ".agent-status"
    return Path.cwd() / ".agent-status"


def make_event(agent: str, story: str, state: str, message: str, ts: datetime | None = None,
               **extra) -> dict:
    if not AGENT_RE.match(agent or "") or agent.endswith(".inbox"):
        raise ValueError(f"invalid agent name {agent!r} (letters, digits, . _ -; max 64)")
    if state not in KNOWN_STATES:
        raise ValueError(f"unknown state {state!r}; expected one of {', '.join(KNOWN_STATES)}")
    msg = " ".join((message or "").split())
    if len(msg) > MAX_MESSAGE:
        msg = msg[: MAX_MESSAGE - 3] + "..."
    ev = {
        "ts": format_ts(ts or datetime.now(timezone.utc)),
        "agent": agent,
        "story": story or "",
        "state": state,
        "message": msg,
    }
    for k, v in extra.items():
        if v is not None and k not in ev:
            ev[k] = v
    return ev


def _append_windows(path: Path, data: bytes) -> None:
    """Atomic append on Windows. The C runtime's O_APPEND is seek-then-write (two
    concurrent writers can overwrite each other); a handle opened with only
    FILE_APPEND_DATA makes the kernel position every write at end-of-file atomically."""
    import ctypes
    from ctypes import wintypes
    k32 = ctypes.WinDLL("kernel32", use_last_error=True)
    k32.CreateFileW.restype = wintypes.HANDLE
    k32.CreateFileW.argtypes = [wintypes.LPCWSTR, wintypes.DWORD, wintypes.DWORD, wintypes.LPVOID,
                                wintypes.DWORD, wintypes.DWORD, wintypes.HANDLE]
    k32.WriteFile.argtypes = [wintypes.HANDLE, wintypes.LPCVOID, wintypes.DWORD,
                              ctypes.POINTER(wintypes.DWORD), wintypes.LPVOID]
    k32.CloseHandle.argtypes = [wintypes.HANDLE]
    FILE_APPEND_DATA, SHARE_ALL, OPEN_ALWAYS, NORMAL = 0x0004, 0x7, 4, 0x80
    h = k32.CreateFileW(str(path), FILE_APPEND_DATA, SHARE_ALL, None, OPEN_ALWAYS, NORMAL, None)
    if h in (None, wintypes.HANDLE(-1).value):
        raise ctypes.WinError(ctypes.get_last_error())
    try:
        written = wintypes.DWORD(0)
        if not k32.WriteFile(h, data, len(data), ctypes.byref(written), None) or written.value != len(data):
            raise ctypes.WinError(ctypes.get_last_error())
    finally:
        k32.CloseHandle(h)


def append_event(path: Path, event: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    data = (json.dumps(event, ensure_ascii=False, separators=(",", ":")) + "\n").encode("utf-8")
    if os.name == "nt":
        _append_windows(path, data)
        return
    flags = os.O_WRONLY | os.O_APPEND | os.O_CREAT
    fd = os.open(path, flags, 0o644)
    try:
        os.write(fd, data)
    finally:
        os.close(fd)


def emit(agent: str, story: str, state: str, message: str = "", *, status_dir: Path | None = None,
         **extra) -> dict:
    ev = make_event(agent, story, state, message, **extra)
    append_event(Path(status_dir or default_status_dir()) / f"{agent}.jsonl", ev)
    return ev


# --- two-way channel (agent side) -----------------------------------------------------

def next_qid(status_dir: Path, agent: str) -> str:
    """q1, q2, ... per agent, from the questions already in its status file."""
    from .inbox import status_path
    n = 0
    try:
        with open(status_path(status_dir, agent), encoding="utf-8", errors="replace") as f:
            for line in f:
                if '"question"' in line:
                    try:
                        obj = json.loads(line)
                    except ValueError:
                        continue
                    q = obj.get("qid") if isinstance(obj, dict) else None
                    if isinstance(q, str) and q[:1] == "q" and q[1:].isdigit():
                        n = max(n, int(q[1:]))
    except OSError:
        pass
    return f"q{n + 1}"


def ask(agent: str, story: str, question: str, *, status_dir: Path | None = None, qid: str | None = None,
        options: list[str] | None = None, default: str | None = None, blocking: bool = False,
        **extra) -> dict:
    status_dir = Path(status_dir or default_status_dir())
    qid = qid or next_qid(status_dir, agent)
    return emit(agent, story, "question", question, status_dir=status_dir, qid=qid,
                options=options or None, default=default, blocking=blocking or None, **extra)


def check_inbox(agent: str, story: str = "", *, status_dir: Path | None = None) -> list:
    """Return pending messages and acknowledge them (one ``ack`` event)."""
    from .inbox import pending
    status_dir = Path(status_dir or default_status_dir())
    msgs = pending(status_dir, agent)
    if msgs:
        emit(agent, story, "ack", f"received {len(msgs)} message(s)", status_dir=status_dir,
             ref=[m.id for m in msgs])
    return msgs


def wait_for_answer(agent: str, qid: str, story: str = "", *, status_dir: Path | None = None,
                    timeout: float = 300.0, poll: float = 2.0, clock=None, sleep=None) -> list:
    """Block (cheaply: no model tokens while waiting) until an answer to ``qid`` -- or a
    ``stop`` -- arrives, or ``timeout`` elapses. Returns the messages delivered (acked)."""
    import time as _time
    from .inbox import pending
    clock = clock or _time.monotonic
    sleep = sleep or _time.sleep
    status_dir = Path(status_dir or default_status_dir())
    deadline = clock() + timeout
    while True:
        msgs = pending(status_dir, agent)
        if any((m.kind == "answer" and m.ref == qid) or m.kind == "stop" for m in msgs):
            return check_inbox(agent, story, status_dir=status_dir)
        if clock() >= deadline:
            return []
        sleep(min(poll, max(deadline - clock(), 0)))


def _git_context() -> dict:
    from .gitinfo import _run
    out = {}
    branch = _run(["git", "rev-parse", "--abbrev-ref", "HEAD"], cwd=Path.cwd())
    top = _run(["git", "rev-parse", "--show-toplevel"], cwd=Path.cwd())
    if branch and branch.strip() != "HEAD":
        out["branch"] = branch.strip()
    if top:
        out["worktree"] = top.strip()
    return out


CLI_ACTIONS = ("check", "wait")
USER_STATES = tuple(s for s in KNOWN_STATES if s != "ack")  # ack is written by check/wait


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(
        prog="emit", description="Append one agent status event; print any new inbox messages.",
        epilog="question: ask the user (prints qid=...).  check: print + acknowledge new inbox "
               "messages.  wait --qid Q: block until Q is answered (exit 3 on timeout).")
    ap.add_argument("state", choices=USER_STATES + CLI_ACTIONS)
    ap.add_argument("message", nargs="?", default="")
    ap.add_argument("--qid", help="question id (question: default next q<N>; wait: required)")
    ap.add_argument("--option", action="append", dest="options", help="question: a possible answer (repeatable)")
    ap.add_argument("--default", help="question: what you will do if nobody answers")
    ap.add_argument("--blocking", action="store_true", help="question: you cannot continue without an answer")
    ap.add_argument("--closes", help="this event resolves question QID (e.g. you found the answer yourself)")
    ap.add_argument("--timeout", type=float, default=300.0, help="wait: seconds (default 300)")
    ap.add_argument("--no-inbox", action="store_true", help="don't check the inbox after writing")
    ap.add_argument("-a", "--agent", default=os.environ.get("AGENT_NAME"),
                    help="agent name (default: $AGENT_NAME)")
    ap.add_argument("-s", "--story", default=os.environ.get("AGENT_STORY", ""),
                    help="story ID (default: $AGENT_STORY)")
    ap.add_argument("-p", "--percent", type=int, help="0-100 progress within the story")
    ap.add_argument("--pr", type=int, help="PR number")
    ap.add_argument("--detail", help="longer free text, not shown in the table")
    ap.add_argument("--dir", type=Path, help="status directory (default: auto)")
    ap.add_argument("--no-git", action="store_true", help="don't add branch/worktree fields")
    ap.add_argument("--print", action="store_true", help="also print the written line")
    args = ap.parse_args(argv)
    if not args.agent:
        ap.error("--agent is required (or set AGENT_NAME)")
    if args.percent is not None and not 0 <= args.percent <= 100:
        ap.error("--percent must be 0-100")
    if not AGENT_RE.match(args.agent):
        ap.error(f"invalid agent name {args.agent!r}")
    from .inbox import format_for_agent
    status_dir = Path(args.dir or default_status_dir())

    if args.state == "check":
        print(format_for_agent(args.agent, check_inbox(args.agent, args.story, status_dir=status_dir))
              or "[agent-monitor inbox] no new messages")
        return 0
    if args.state == "wait":
        if not args.qid:
            ap.error("wait needs --qid")
        msgs = wait_for_answer(args.agent, args.qid, args.story, status_dir=status_dir, timeout=args.timeout)
        if not msgs:
            print(f"[agent-monitor inbox] no answer to {args.qid} after {args.timeout:g}s; "
                  "continue with your stated default or ask again later")
            return 3
        print(format_for_agent(args.agent, msgs))
        return 0

    extra = {} if args.no_git else _git_context()
    extra.update(percent=args.percent, pr=args.pr, detail=args.detail, closes=args.closes)
    try:
        if args.state == "question":
            if not args.message:
                ap.error("question needs the question text")
            ev = ask(args.agent, args.story, args.message, status_dir=status_dir, qid=args.qid,
                     options=args.options, default=args.default, blocking=args.blocking, **extra)
            print(f"qid={ev['qid']}")
        else:
            ev = emit(args.agent, args.story, args.state, args.message, status_dir=status_dir, **extra)
    except ValueError as exc:
        ap.error(str(exc))
    if args.print:
        print(json.dumps(ev, ensure_ascii=True))
    if not args.no_inbox:
        # Piggy-back delivery: zero extra tool calls, zero output when the inbox is empty.
        text = format_for_agent(args.agent, check_inbox(args.agent, args.story, status_dir=status_dir))
        if text:
            print(text)
    return 0


if __name__ == "__main__":
    sys.exit(main())
