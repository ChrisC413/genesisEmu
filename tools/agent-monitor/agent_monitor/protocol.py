"""Agent status event protocol (spec 17, "Event protocol").

One event per line, JSON object, UTF-8, appended to ``.agent-status/<agent>.jsonl``.

v1 fields (all producers write these)::

    ts       ISO-8601 timestamp, UTC ("2026-10-24T11:22:33Z")
    agent    agent name (normally equal to the file stem)
    story    story ID being worked on ("M00-04"), may be empty
    state    started | progress | blocked | done | failed
    message  one line of human-readable text

Optional extensions (readers must ignore unknown fields)::

    state "heartbeat"   liveness only; does not change the displayed state
    percent             0-100, progress within the current story
    branch, worktree    git context of the agent
    pr                  PR number the agent is working on
    files_changed       integer
    tokens, cost_usd    usage, when the agent can obtain it
    detail              longer free text (not shown in the table)
    closes              question ID (or list) this event resolves

Two-way channel (docs/specs/03-two-way-channel.md)::

    state "question"    agent asks; fields qid, options, default, blocking
    state "ack"         agent received inbox messages; field ref = [message ids]
"""

from __future__ import annotations

import json
from dataclasses import dataclass, field
from datetime import datetime, timezone

V1_STATES = ("started", "progress", "blocked", "done", "failed")
# Extension states that do not change the agent's displayed state ("side-channel" events).
SIDE_STATES = ("heartbeat", "question", "ack")
EXT_STATES = SIDE_STATES
KNOWN_STATES = V1_STATES + EXT_STATES
TERMINAL_STATES = ("done", "failed")
CORE_FIELDS = ("ts", "agent", "story", "state", "message")


class ParseError(ValueError):
    """A line that is not a valid event. ``reason`` is a short explanation."""

    def __init__(self, reason: str, line: str, source: str | None = None, lineno: int | None = None):
        super().__init__(reason)
        self.reason = reason
        self.line = line
        self.source = source
        self.lineno = lineno

    def __str__(self) -> str:
        where = f"{self.source}:{self.lineno}" if self.source else "<input>"
        return f"{where}: {self.reason}"


@dataclass(frozen=True)
class Event:
    ts: datetime
    agent: str
    story: str
    state: str
    message: str
    extra: dict = field(default_factory=dict)
    source: str | None = None
    lineno: int | None = None

    @property
    def known_state(self) -> bool:
        return self.state in KNOWN_STATES

    def to_dict(self) -> dict:
        d = {
            "ts": format_ts(self.ts),
            "agent": self.agent,
            "story": self.story,
            "state": self.state,
            "message": self.message,
        }
        d.update(self.extra)
        return d


def parse_ts(value: str) -> datetime:
    """Parse an ISO-8601 timestamp. Naive timestamps are taken as UTC."""
    if not isinstance(value, str) or not value:
        raise ValueError("ts must be a non-empty string")
    text = value.strip()
    if text[-1:] in ("Z", "z"):  # fromisoformat accepts "Z" only from Python 3.11
        text = text[:-1] + "+00:00"
    dt = datetime.fromisoformat(text)
    if dt.tzinfo is None:
        dt = dt.replace(tzinfo=timezone.utc)
    return dt.astimezone(timezone.utc)


def format_ts(dt: datetime) -> str:
    return dt.astimezone(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


def parse_line(line: str, source: str | None = None, lineno: int | None = None,
               default_agent: str | None = None) -> Event:
    """Parse one line into an :class:`Event` or raise :class:`ParseError`.

    ``default_agent`` (usually the file stem) is used when the line has no ``agent``.
    """
    text = line.strip().lstrip("﻿")
    if not text:
        raise ParseError("empty line", line, source, lineno)
    try:
        obj = json.loads(text)
    except json.JSONDecodeError as exc:
        raise ParseError(f"invalid JSON ({exc.msg})", line, source, lineno) from None
    if not isinstance(obj, dict):
        raise ParseError("not a JSON object", line, source, lineno)

    if "ts" not in obj:
        raise ParseError("missing 'ts'", line, source, lineno)
    try:
        ts = parse_ts(obj["ts"])
    except (ValueError, TypeError) as exc:
        raise ParseError(f"bad 'ts' ({exc})", line, source, lineno) from None

    agent = obj.get("agent") or default_agent
    if not isinstance(agent, str) or not agent:
        raise ParseError("missing 'agent'", line, source, lineno)

    state = obj.get("state")
    if not isinstance(state, str) or not state:
        raise ParseError("missing 'state'", line, source, lineno)

    story = obj.get("story") or ""
    message = obj.get("message") or ""
    if not isinstance(story, str):
        story = str(story)
    if not isinstance(message, str):
        message = str(message)
    message = " ".join(message.split())  # contract: one line

    extra = {k: v for k, v in obj.items() if k not in CORE_FIELDS}
    return Event(ts=ts, agent=agent, story=story, state=state.lower(), message=message,
                 extra=extra, source=source, lineno=lineno)
