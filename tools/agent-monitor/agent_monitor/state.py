"""Aggregates events into one row per agent, open questions, inbox state, and a feed."""

from __future__ import annotations

from collections import deque
from dataclasses import dataclass, field
from datetime import datetime, timedelta, timezone
from pathlib import Path

from .inbox import Message, agent_of_inbox, is_inbox_file, parse_inbox_line
from .protocol import SIDE_STATES, TERMINAL_STATES, Event, ParseError, parse_line

# Liveness classes.
LIVE_OK = "ok"
LIVE_QUIET = "quiet"     # no event for warn_after
LIVE_STALE = "stale"     # no event for stale_after -- agent may be stuck or dead
LIVE_IDLE = "idle"       # last state is done/failed: silence is expected
LIVE_ASK = "ask"         # has an unanswered blocking question -- needs the user

# Question status.
Q_OPEN = "open"            # nobody answered yet
Q_ANSWERED = "answered"    # an answer is in the inbox, agent hasn't picked it up
Q_DELIVERED = "delivered"  # agent acknowledged the answer
Q_CLOSED = "closed"        # agent resolved it itself (``closes``)


def _ids(value) -> list[str]:
    if isinstance(value, str):
        return [value] if value else []
    if isinstance(value, list):
        return [v for v in value if isinstance(v, str)]
    return []


@dataclass
class Question:
    qid: str
    text: str
    ts: datetime
    story: str = ""
    options: list = field(default_factory=list)
    default: str = ""
    blocking: bool = False
    closed: bool = False


@dataclass
class AgentState:
    name: str
    story: str = ""
    state: str = ""
    message: str = ""
    last_ts: datetime | None = None          # newest event of any kind (incl. heartbeat)
    last_change_ts: datetime | None = None   # newest event that changed state/message
    first_ts: datetime | None = None
    events: int = 0
    stories_done: list[str] = field(default_factory=list)
    extra: dict = field(default_factory=dict)  # latest value of each extension field
    questions: dict[str, Question] = field(default_factory=dict)
    acked: set[str] = field(default_factory=set)
    # Filled by Monitor from the inbox file:
    messages: list[Message] = field(default_factory=list)
    relayed: set[str] = field(default_factory=set)

    # --- events -------------------------------------------------------------------
    def apply(self, ev: Event) -> None:
        self.events += 1
        if self.first_ts is None or ev.ts < self.first_ts:
            self.first_ts = ev.ts
        if self.last_ts is None or ev.ts >= self.last_ts:
            self.last_ts = ev.ts
        for qid in _ids(ev.extra.get("closes")):
            if qid in self.questions:
                self.questions[qid].closed = True
        if ev.state in SIDE_STATES:
            self._apply_side(ev)
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

    def _apply_side(self, ev: Event) -> None:
        if ev.state == "heartbeat":
            if ev.story and not self.story:
                self.story = ev.story
            self._merge_extra(ev)
        elif ev.state == "question":
            qid = ev.extra.get("qid")
            if not isinstance(qid, str) or not qid:
                qid = f"?{len(self.questions) + 1}"
            opts = ev.extra.get("options")
            self.questions[qid] = Question(
                qid=qid, text=ev.message, ts=ev.ts, story=ev.story,
                options=[str(o) for o in opts] if isinstance(opts, list) else [],
                default=str(ev.extra.get("default") or ""), blocking=bool(ev.extra.get("blocking")))
        elif ev.state == "ack":
            self.acked.update(_ids(ev.extra.get("ref")))

    def _merge_extra(self, ev: Event) -> None:
        for k, v in ev.extra.items():
            self.extra[k] = v

    # --- inbox --------------------------------------------------------------------
    def delivered(self, msg_id: str) -> bool:
        return msg_id in self.acked or msg_id in self.relayed

    def pending_messages(self) -> list[Message]:
        return [m for m in self.messages if not self.delivered(m.id)]

    def answer_for(self, qid: str) -> Message | None:
        found = None
        for m in self.messages:
            if m.kind == "answer" and m.ref == qid:
                found = m
        return found

    def question_status(self, qid: str) -> str:
        q = self.questions.get(qid)
        if q is not None and q.closed:
            return Q_CLOSED
        ans = self.answer_for(qid)
        if ans is None:
            return Q_OPEN
        return Q_DELIVERED if self.delivered(ans.id) else Q_ANSWERED

    def open_questions(self) -> list[Question]:
        """Questions still needing attention: unanswered, or answered but not yet picked up."""
        return [q for q in self.questions.values() if self.question_status(q.qid) in (Q_OPEN, Q_ANSWERED)]

    # --- liveness -----------------------------------------------------------------
    def age(self, now: datetime) -> timedelta | None:
        return None if self.last_ts is None else max(now - self.last_ts, timedelta(0))

    def liveness(self, now: datetime, warn_after: timedelta, stale_after: timedelta) -> str:
        if self.state in TERMINAL_STATES:
            return LIVE_IDLE
        if any(q.blocking and self.question_status(q.qid) == Q_OPEN for q in self.questions.values()):
            return LIVE_ASK
        age = self.age(now)
        if age is None or age >= stale_after:
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

    def agent(self, name: str) -> AgentState:
        return self.agents.setdefault(name, AgentState(name))

    def ingest_line(self, text: str, source: str | None = None, lineno: int | None = None) -> Event | None:
        if not text.strip():
            return None  # blank lines are not errors
        if source and is_inbox_file(source):
            self._ingest_inbox(text, source, lineno)
            return None
        default_agent = Path(source).stem if source else None
        try:
            ev = parse_line(text, source, lineno, default_agent=default_agent)
        except ParseError as err:
            self._error(err)
            return None
        self.ingest(ev)
        return ev

    def _ingest_inbox(self, text: str, source: str, lineno: int | None) -> None:
        name = agent_of_inbox(source)
        try:
            msg, ids = parse_inbox_line(text, name)
        except (ValueError, TypeError) as exc:
            self._error(ParseError(f"inbox: {exc}", text, source, lineno))
            return
        # Inbox lines don't create a visible agent until it has reported at least once,
        # but the state object holds the messages for when it does.
        a = self.agent(name)
        if msg:
            a.messages.append(msg)
        a.relayed.update(ids)

    def _error(self, err: ParseError) -> None:
        self.errors.append(err)
        self.error_count += 1

    def ingest(self, ev: Event) -> None:
        self.agent(ev.agent).apply(ev)
        if ev.state not in ("heartbeat", "ack"):
            self.feed.append(ev)

    def recent(self, n: int) -> list[Event]:
        """The newest ``n`` feed events, oldest first."""
        events = sorted(self.feed, key=lambda e: e.ts)
        return events[-n:] if n > 0 else []

    def rows(self) -> list[AgentState]:
        """Agents that have reported, most recently active first."""
        epoch = datetime.min.replace(tzinfo=timezone.utc)
        return sorted((a for a in self.agents.values() if a.events),
                      key=lambda a: a.last_ts or epoch, reverse=True)

    def open_questions(self) -> list[tuple[AgentState, Question]]:
        out = [(a, q) for a in self.rows() for q in a.open_questions()]
        return sorted(out, key=lambda aq: (not aq[1].blocking, aq[1].ts))
