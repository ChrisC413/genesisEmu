"""Two-way channel: per-agent inbox of messages from the user (docs/specs/03-two-way-channel.md).

``.agent-status/<agent>.inbox.jsonl`` -- append-only, several writers (the user via
``monitor.py``, a coordinator session relaying, ...). Line kinds::

    {"ts","id":"m-...","to":"<agent>","from":"chris","kind":"answer","ref":"q1","text":"..."}
    {"ts","id":"m-...","to":"<agent>","from":"chris","kind":"steer|note|stop","text":"..."}
    {"ts","kind":"delivered","ref":["m-..."],"via":"sendmessage","from":"coordinator"}

A message is *delivered* when the agent acknowledged it (``state:"ack"`` event with
``ref`` in its own status file) or a relayer recorded a ``delivered`` line for it.
No cursor files: delivery state is derived from the two logs, so it survives crashes.
"""

from __future__ import annotations

import json
import os
import secrets
from dataclasses import dataclass, field
from datetime import datetime, timezone
from pathlib import Path

from .protocol import format_ts, parse_line, parse_ts

MESSAGE_KINDS = ("answer", "steer", "note", "stop")
INBOX_SUFFIX = ".inbox.jsonl"
MAX_TEXT = 4000


def inbox_path(status_dir: Path, agent: str) -> Path:
    return Path(status_dir) / f"{agent}{INBOX_SUFFIX}"


def status_path(status_dir: Path, agent: str) -> Path:
    return Path(status_dir) / f"{agent}.jsonl"


def is_inbox_file(path: str | Path) -> bool:
    return str(path).endswith(INBOX_SUFFIX)


def agent_of_inbox(path: str | Path) -> str:
    return Path(path).name[: -len(INBOX_SUFFIX)]


def new_message_id(now: datetime | None = None) -> str:
    now = now or datetime.now(timezone.utc)
    return f"m-{now:%Y%m%d%H%M%S}-{secrets.token_hex(2)}"


@dataclass
class Message:
    id: str
    ts: datetime
    to: str
    sender: str
    kind: str
    text: str
    ref: str = ""
    extra: dict = field(default_factory=dict)

    def to_dict(self) -> dict:
        d = {"ts": format_ts(self.ts), "id": self.id, "to": self.to, "from": self.sender,
             "kind": self.kind, "text": self.text}
        if self.ref:
            d["ref"] = self.ref
        d.update(self.extra)
        return d


def _refs(value) -> list[str]:
    if isinstance(value, str):
        return [value] if value else []
    if isinstance(value, list):
        return [v for v in value if isinstance(v, str) and v]
    return []


def parse_inbox_line(text: str, agent: str) -> tuple[Message | None, list[str]]:
    """Returns (message, delivered_ids). Raises ValueError for malformed lines."""
    obj = json.loads(text)
    if not isinstance(obj, dict):
        raise ValueError("not a JSON object")
    kind = obj.get("kind")
    if kind == "delivered":
        return None, _refs(obj.get("ref"))
    if kind not in MESSAGE_KINDS:
        raise ValueError(f"unknown kind {kind!r}")
    mid = obj.get("id")
    if not isinstance(mid, str) or not mid:
        raise ValueError("missing 'id'")
    known = {"ts", "id", "to", "from", "kind", "text", "ref"}
    msg = Message(id=mid, ts=parse_ts(obj.get("ts", "")), to=obj.get("to") or agent,
                  sender=str(obj.get("from") or "?"), kind=kind, text=str(obj.get("text") or ""),
                  ref=str(obj.get("ref") or ""), extra={k: v for k, v in obj.items() if k not in known})
    return msg, []


def _append(path: Path, obj: dict) -> None:
    from .emit import append_event  # atomic append (shared with status events)
    append_event(path, obj)


def post(status_dir: Path, agent: str, kind: str, text: str, *, ref: str = "",
         sender: str | None = None, now: datetime | None = None) -> Message:
    if kind not in MESSAGE_KINDS:
        raise ValueError(f"kind must be one of {', '.join(MESSAGE_KINDS)}")
    text = (text or "").strip()
    if kind != "stop" and not text:
        raise ValueError("message text is empty")
    if kind == "answer" and not ref:
        raise ValueError("an answer needs the question id (ref)")
    if len(text) > MAX_TEXT:
        raise ValueError(f"message longer than {MAX_TEXT} characters")
    now = now or datetime.now(timezone.utc)
    msg = Message(id=new_message_id(now), ts=now, to=agent, kind=kind, text=text, ref=ref,
                  sender=sender or default_sender())
    _append(inbox_path(status_dir, agent), msg.to_dict())
    return msg


def mark_delivered(status_dir: Path, agent: str, ids: list[str], via: str, sender: str = "relay") -> None:
    if ids:
        _append(inbox_path(status_dir, agent), {"ts": format_ts(datetime.now(timezone.utc)),
                                                 "kind": "delivered", "ref": ids, "via": via,
                                                 "from": sender})


def default_sender() -> str:
    env = os.environ.get("AGENT_MONITOR_USER")
    if env:
        return env
    try:
        import getpass
        return getpass.getuser()
    except Exception:
        return "user"


def _read_lines(path: Path) -> list[str]:
    try:
        return path.read_text(encoding="utf-8", errors="replace").splitlines()
    except OSError:
        return []


def read_inbox(status_dir: Path, agent: str) -> tuple[list[Message], set[str]]:
    """All messages (oldest first) and the ids delivered according to the inbox itself."""
    msgs, delivered = [], set()
    for line in _read_lines(inbox_path(status_dir, agent)):
        if not line.strip():
            continue
        try:
            msg, ids = parse_inbox_line(line, agent)
        except (ValueError, TypeError):
            continue
        if msg:
            msgs.append(msg)
        delivered.update(ids)
    return msgs, delivered


def acked_ids(status_dir: Path, agent: str) -> set[str]:
    out: set[str] = set()
    for line in _read_lines(status_path(status_dir, agent)):
        if '"ack"' not in line:
            continue
        try:
            ev = parse_line(line, default_agent=agent)
        except ValueError:
            continue
        if ev.state == "ack":
            out.update(_refs(ev.extra.get("ref")))
    return out


def pending(status_dir: Path, agent: str) -> list[Message]:
    msgs, delivered = read_inbox(status_dir, agent)
    delivered |= acked_ids(status_dir, agent)
    return [m for m in msgs if m.id not in delivered]


def format_for_agent(agent: str, msgs: list[Message]) -> str:
    """Compact text an agent reads in its tool output. Empty string when nothing is pending."""
    if not msgs:
        return ""
    lines = [f"[agent-monitor inbox] {len(msgs)} new message(s) for {agent} (now acknowledged):"]
    for m in msgs:
        what = f"answer to {m.ref}" if m.kind == "answer" else m.kind
        body = m.text or "(no text)"
        lines.append(f"- {m.id} {what} from {m.sender}: {body}")
    if any(m.kind == "stop" for m in msgs):
        lines.append("A 'stop' message asks you to finish the current step, report status, and end.")
    if any(m.kind == "steer" for m in msgs):
        lines.append("Apply 'steer' messages at your next checkpoint and confirm with a progress event.")
    return "\n".join(lines)
