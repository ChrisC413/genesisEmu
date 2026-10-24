"""Aggregates events into one row per agent plus a recent-events feed."""

from __future__ import annotations

from collections import deque
from dataclasses import dataclass, field
from datetime import datetime, timedelta, timezone
from pathlib import Path

from .protocol import TERMINAL_STATES, Event, ParseError, parse_line

# Liveness classes, in increasing severity.
LIVE_OK = "ok"
LIVE_QUIET = "quiet"     # no event for warn_after
LIVE_STALE = "stale"     # no event for stale_after -- agent may be stuck or dead
LIVE_IDLE = "idle"       # last state is done/failed: silence is expected


@dataclass
class AgentState:
    name: str
    story: str = ""
    state: str = ""
    message: str = ""
    last_ts: datetime | None = None          # newest event of any kind (incl. heartbeat)
    last_change_ts: datetime | None = None   # newest non-heartbeat event
    first_ts: datetime | None = None
    events: int = 0
    stories_done: list[str] = field(default_factory=list)
    extra: dict = field(default_factory=dict)  # latest value of each extension field

    def apply(self, ev: Event) -> None:
        self.events += 1
        if self.first_ts is None or ev.ts < self.first_ts:
            self.first_ts = ev.ts
        if self.last_ts is None or ev.ts >= self.last_ts:
            self.last_ts = ev.ts
        if ev.state == "heartbeat":
            if ev.story and not self.story:
                self.story = ev.story
            self._merge_extra(ev)
            return
        # Out-of-order (older) events are kept in the feed but don't override the row.
        if self.last_change_ts is not None and ev.ts < self.last_change_ts:
            return
        self.last_change_ts = ev.ts
        if ev.story and ev.story != self.story:
            self.extra.pop("percent", None)  # percent is per story
        self.story = ev.story or self.story
        self.state = ev.state
        self.message = ev.message
        self._merge_extra(ev)
        if ev.state == "done" and ev.story and ev.story not in self.stories_done:
            self.stories_done.append(ev.story)

    def _merge_extra(self, ev: Event) -> None:
        for k, v in ev.extra.items():
            self.extra[k] = v

    def age(self, now: datetime) -> timedelta | None:
        return None if self.last_ts is None else max(now - self.last_ts, timedelta(0))

    def liveness(self, now: datetime, warn_after: timedelta, stale_after: timedelta) -> str:
        if self.state in TERMINAL_STATES:
            return LIVE_IDLE
        age = self.age(now)
        if age is None:
            return LIVE_STALE
        if age >= stale_after:
            return LIVE_STALE
        if age >= warn_after:
            return LIVE_QUIET
        return LIVE_OK


class Monitor:
    def __init__(self, feed_size: int = 200, error_size: int = 50):
        self.agents: dict[str, AgentState] = {}
        self.feed: deque[Event] = deque(maxlen=feed_size)
        self.errors: deque[ParseError] = deque(maxlen=error_size)
        self.error_count = 0

    def ingest_line(self, text: str, source: str | None = None, lineno: int | None = None) -> Event | None:
        if not text.strip():
            return None  # blank lines are not errors
        default_agent = Path(source).stem if source else None
        try:
            ev = parse_line(text, source, lineno, default_agent=default_agent)
        except ParseError as err:
            self.errors.append(err)
            self.error_count += 1
            return None
        self.ingest(ev)
        return ev

    def ingest(self, ev: Event) -> None:
        self.agents.setdefault(ev.agent, AgentState(ev.agent)).apply(ev)
        if ev.state != "heartbeat":
            self.feed.append(ev)

    def recent(self, n: int) -> list[Event]:
        """The newest ``n`` feed events, oldest first."""
        events = sorted(self.feed, key=lambda e: e.ts)
        return events[-n:] if n > 0 else []

    def rows(self) -> list[AgentState]:
        """Agents, most recently active first."""
        epoch = datetime.min.replace(tzinfo=timezone.utc)
        return sorted(self.agents.values(), key=lambda a: a.last_ts or epoch, reverse=True)
