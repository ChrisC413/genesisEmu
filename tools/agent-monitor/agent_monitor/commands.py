"""User-side subcommands of ``monitor.py`` (two-way channel; backlog; issues plan)."""

from __future__ import annotations

import argparse
import json
import sys
from datetime import datetime, timezone
from pathlib import Path

from . import inbox
from .protocol import format_ts
from .state import Q_OPEN, Monitor
from .tail import DirTailer

COMMANDS = ("answer", "steer", "note", "stop", "questions", "inbox", "pending", "mark-delivered",
            "backlog", "issues-plan")


def load_monitor(status_dir: Path) -> Monitor:
    mon = Monitor()
    for ln in DirTailer(status_dir, partial_grace=0).poll():
        mon.ingest_line(ln.text, ln.source, ln.lineno)
    return mon


def _status_dir(args) -> Path:
    from .emit import default_status_dir
    return Path(args.dir) if args.dir else default_status_dir()


def cmd_answer(args) -> int:
    sd = _status_dir(args)
    mon = load_monitor(sd)
    a = mon.agents.get(args.agent)
    if not args.force:
        if a is None or not a.events:
            print(f"error: no agent named {args.agent!r} has reported (use --force to send anyway)", file=sys.stderr)
            return 2
        if args.qid not in a.questions:
            known = ", ".join(a.questions) or "none"
            print(f"error: {args.agent} has no question {args.qid!r} (known: {known}; --force to send anyway)",
                  file=sys.stderr)
            return 2
        status = a.question_status(args.qid)
        if status != Q_OPEN:
            print(f"note: {args.qid} is already {status}; sending another answer", file=sys.stderr)
    msg = inbox.post(sd, args.agent, "answer", args.text, ref=args.qid, sender=args.sender)
    print(f"queued {msg.id} for {args.agent} (answer to {args.qid}); delivered at the agent's next check")
    return 0


def cmd_message(args) -> int:
    sd = _status_dir(args)
    if not args.force:
        mon = load_monitor(sd)
        if args.agent not in mon.agents or not mon.agents[args.agent].events:
            print(f"error: no agent named {args.agent!r} has reported (use --force to send anyway)", file=sys.stderr)
            return 2
    msg = inbox.post(sd, args.agent, args.cmd, args.text or "", sender=args.sender)
    print(f"queued {msg.id} for {args.agent} ({args.cmd}); delivered at the agent's next check")
    return 0


def cmd_questions(args) -> int:
    mon = load_monitor(_status_dir(args))
    rows = []
    for a, q in mon.open_questions():
        rows.append({"agent": a.name, "qid": q.qid, "status": a.question_status(q.qid), "blocking": q.blocking,
                     "story": q.story, "asked": format_ts(q.ts), "text": q.text, "options": q.options,
                     "default": q.default})
    if args.json:
        print(json.dumps(rows, indent=2))
        return 0
    if not rows:
        print("no open questions")
    for r in rows:
        flag = "BLOCKING" if r["blocking"] else r["status"]
        print(f"{r['agent']} {r['qid']} [{flag}] {r['story'] or '-'}: {r['text']}")
        if r["options"]:
            print(f"    options: {' | '.join(r['options'])}")
        if r["default"]:
            print(f"    default: {r['default']}")
    return 0


def cmd_inbox(args) -> int:
    mon = load_monitor(_status_dir(args))
    a = mon.agents.get(args.agent)
    if a is None or not a.messages:
        print(f"no messages for {args.agent}")
        return 0
    for m in a.messages:
        state = "delivered" if a.delivered(m.id) else "pending"
        what = f"answer {m.ref}" if m.kind == "answer" else m.kind
        print(f"{format_ts(m.ts)} {m.id} [{state}] {what} from {m.sender}: {m.text}")
    return 0


def cmd_pending(args) -> int:
    """Undelivered messages, for a relayer (e.g. a coordinator using SendMessage)."""
    mon = load_monitor(_status_dir(args))
    rows = [{"agent": a.name, **m.to_dict()} for a in mon.agents.values() for m in a.pending_messages()]
    if args.json:
        print(json.dumps(rows, indent=2))
    elif not rows:
        print("no pending messages")
    else:
        for r in rows:
            print(f"{r['agent']} {r['id']} {r['kind']}{' ' + r['ref'] if r.get('ref') else ''}: {r['text']}")
    return 0


def cmd_mark_delivered(args) -> int:
    inbox.mark_delivered(_status_dir(args), args.agent, args.ids, via=args.via, sender=args.sender)
    print(f"marked {len(args.ids)} message(s) delivered to {args.agent} via {args.via}")
    return 0


def cmd_backlog(args) -> int:
    from .backlog import main_backlog
    return main_backlog(args, load_monitor(_status_dir(args)))


def cmd_issues_plan(args) -> int:
    from .issues import main_issues_plan
    return main_issues_plan(args)


def build_parser() -> argparse.ArgumentParser:
    ap = argparse.ArgumentParser(prog="monitor", description="Agent monitor: talk to agents, see backlogs.")
    common = argparse.ArgumentParser(add_help=False)
    common.add_argument("--dir", help="status directory (default: auto)")
    common.add_argument("--from", dest="sender", default=None, help="sender name (default: $AGENT_MONITOR_USER or login)")
    sub = ap.add_subparsers(dest="cmd", required=True)

    p = sub.add_parser("answer", parents=[common], help="answer an agent's question")
    p.add_argument("agent"), p.add_argument("qid"), p.add_argument("text")
    p.add_argument("--force", action="store_true", help="send even if the question is unknown")
    p.set_defaults(fn=cmd_answer)
    for kind, helptext in (("steer", "change what an agent is doing (applied at its next checkpoint)"),
                           ("note", "FYI for an agent; no action expected"),
                           ("stop", "ask an agent to finish its current step and end")):
        p = sub.add_parser(kind, parents=[common], help=helptext)
        p.add_argument("agent")
        p.add_argument("text", nargs="?" if kind == "stop" else None, default="")
        p.add_argument("--force", action="store_true", help="send even if the agent never reported")
        p.set_defaults(fn=cmd_message)

    p = sub.add_parser("questions", parents=[common], help="list open questions")
    p.add_argument("--json", action="store_true")
    p.set_defaults(fn=cmd_questions)
    p = sub.add_parser("inbox", parents=[common], help="show an agent's inbox and delivery state")
    p.add_argument("agent")
    p.set_defaults(fn=cmd_inbox)
    p = sub.add_parser("pending", parents=[common], help="undelivered messages (for relaying)")
    p.add_argument("--json", action="store_true")
    p.set_defaults(fn=cmd_pending)
    p = sub.add_parser("mark-delivered", parents=[common], help="record that messages were relayed")
    p.add_argument("agent"), p.add_argument("ids", nargs="+")
    p.add_argument("--via", default="sendmessage")
    p.set_defaults(fn=cmd_mark_delivered)

    p = sub.add_parser("backlog", parents=[common], help="milestones and stories of a project, with agents")
    p.add_argument("--project", help="project root or registered name (default: this repo)")
    p.add_argument("--all", action="store_true", help="all registered projects")
    p.add_argument("--json", action="store_true")
    p.add_argument("--milestone", help="only this milestone folder prefix")
    p.set_defaults(fn=cmd_backlog)
    p = sub.add_parser("issues-plan", parents=[common],
                       help="DRY RUN: show what a stories->GitHub Issues sync would do (never writes)")
    p.add_argument("--project", help="project root (default: this repo)")
    p.add_argument("--offline", action="store_true", help="don't query GitHub; assume no issues exist")
    p.set_defaults(fn=cmd_issues_plan)
    return ap


def main(argv: list[str]) -> int:
    args = build_parser().parse_args(argv)
    try:
        return args.fn(args)
    except ValueError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2
