import contextlib
import io
import json
import os
import sys
import tempfile
import unittest
from datetime import datetime, timedelta, timezone
from pathlib import Path

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

from agent_monitor import inbox  # noqa: E402
from agent_monitor.commands import load_monitor, main as cmd_main  # noqa: E402
from agent_monitor.emit import ask, check_inbox, emit, main as emit_main, wait_for_answer  # noqa: E402
from agent_monitor.render import render  # noqa: E402
from agent_monitor.state import (LIVE_ASK, LIVE_OK, Q_ANSWERED, Q_CLOSED, Q_DELIVERED,  # noqa: E402
                                 Q_OPEN)

W, S = timedelta(minutes=5), timedelta(minutes=15)


def run(fn, argv):
    out, err = io.StringIO(), io.StringIO()
    with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
        try:
            rc = fn(argv)
        except SystemExit as exc:
            rc = exc.code
    return rc, out.getvalue(), err.getvalue()


class ChannelTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.d = Path(self.tmp.name)

    def tearDown(self):
        self.tmp.cleanup()

    def status(self, agent="a"):
        a = load_monitor(self.d).agents.get(agent)
        return a

    def test_question_ids_increment_per_agent(self):
        self.assertEqual(ask("a", "M00-04", "Which ROM?", status_dir=self.d)["qid"], "q1")
        self.assertEqual(ask("a", "M00-04", "And region?", status_dir=self.d)["qid"], "q2")
        self.assertEqual(ask("b", "M00-04", "Other agent", status_dir=self.d)["qid"], "q1")
        self.assertEqual(ask("a", "", "explicit", status_dir=self.d, qid="rom")["qid"], "rom")

    def test_question_does_not_change_row_state(self):
        emit("a", "M00-04", "progress", "parsing header", status_dir=self.d)
        ask("a", "M00-04", "Which ROM?", status_dir=self.d)
        a = self.status()
        self.assertEqual((a.state, a.message), ("progress", "parsing header"))
        self.assertEqual(list(a.questions), ["q1"])

    def test_lifecycle_open_answered_delivered(self):
        emit("a", "M00-04", "progress", "x", status_dir=self.d)
        ask("a", "M00-04", "Which ROM?", status_dir=self.d, blocking=True, options=["REV00", "REV01"],
            default="REV01")
        now = datetime.now(timezone.utc)
        a = self.status()
        self.assertEqual(a.question_status("q1"), Q_OPEN)
        self.assertEqual(a.liveness(now, W, S), LIVE_ASK)
        self.assertEqual(a.questions["q1"].options, ["REV00", "REV01"])

        msg = inbox.post(self.d, "a", "answer", "Use REV01", ref="q1", sender="chris")
        a = self.status()
        self.assertEqual(a.question_status("q1"), Q_ANSWERED)
        self.assertEqual(a.liveness(now, W, S), LIVE_OK)  # answered: no longer waiting on the user
        self.assertEqual([m.id for m in a.pending_messages()], [msg.id])

        got = check_inbox("a", "M00-04", status_dir=self.d)
        self.assertEqual([m.text for m in got], ["Use REV01"])
        a = self.status()
        self.assertEqual(a.question_status("q1"), Q_DELIVERED)
        self.assertEqual(a.open_questions(), [])
        self.assertEqual(check_inbox("a", status_dir=self.d), [])  # delivered exactly once
        self.assertEqual(a.state, "progress")  # ack is a side event

    def test_agent_can_close_its_own_question(self):
        emit("a", "", "started", "x", status_dir=self.d)
        ask("a", "", "Need X?", status_dir=self.d, blocking=True)
        emit("a", "", "progress", "found it in the spec", status_dir=self.d, closes="q1")
        a = self.status()
        self.assertEqual(a.question_status("q1"), Q_CLOSED)
        self.assertNotEqual(a.liveness(datetime.now(timezone.utc), W, S), LIVE_ASK)

    def test_relay_marks_delivered_without_agent_ack(self):
        emit("a", "", "started", "x", status_dir=self.d)
        m1 = inbox.post(self.d, "a", "steer", "Do M00-06 before M00-05")
        m2 = inbox.post(self.d, "a", "note", "FYI CI is red on main")
        inbox.mark_delivered(self.d, "a", [m1.id], via="sendmessage", sender="coordinator")
        self.assertEqual([m.id for m in inbox.pending(self.d, "a")], [m2.id])
        self.assertEqual([m.id for m in self.status().pending_messages()], [m2.id])

    def test_emit_cli_piggybacks_inbox_and_is_silent_when_empty(self):
        rc, out, _ = run(emit_main, ["-a", "a", "--dir", str(self.d), "--no-git", "progress", "step 1"])
        self.assertEqual((rc, out), (0, ""))
        inbox.post(self.d, "a", "steer", "Skip the optional refactor", sender="chris")
        rc, out, _ = run(emit_main, ["-a", "a", "--dir", str(self.d), "--no-git", "progress", "step 2"])
        self.assertIn("1 new message(s)", out)
        self.assertIn("steer from chris: Skip the optional refactor", out)
        rc, out, _ = run(emit_main, ["-a", "a", "--dir", str(self.d), "--no-git", "progress", "step 3"])
        self.assertEqual(out, "")

    def test_emit_cli_question_prints_qid(self):
        rc, out, _ = run(emit_main, ["-a", "a", "--dir", str(self.d), "--no-git", "question", "Which ROM?",
                                     "--option", "REV00", "--option", "REV01", "--blocking"])
        self.assertEqual(rc, 0)
        self.assertIn("qid=q1", out)
        line = json.loads((self.d / "a.jsonl").read_text(encoding="utf-8").splitlines()[0])
        self.assertEqual((line["state"], line["qid"], line["options"], line["blocking"]),
                         ("question", "q1", ["REV00", "REV01"], True))

    def test_wait_returns_answer_or_times_out(self):
        ask("a", "", "Which ROM?", status_dir=self.d)
        t = [0.0]
        calls = []

        def sleep(dt):
            t[0] += dt
            calls.append(dt)
            if len(calls) == 3:
                inbox.post(self.d, "a", "answer", "REV01", ref="q1")

        got = wait_for_answer("a", "q1", status_dir=self.d, timeout=60, poll=2, clock=lambda: t[0], sleep=sleep)
        self.assertEqual([m.text for m in got], ["REV01"])
        t[0] = 0.0
        calls.clear()
        self.assertEqual(wait_for_answer("a", "q9", status_dir=self.d, timeout=5, poll=2,
                                         clock=lambda: t[0], sleep=lambda dt: t.__setitem__(0, t[0] + dt)), [])

    def test_wait_cli_timeout_exit_code(self):
        rc, out, _ = run(emit_main, ["-a", "a", "--dir", str(self.d), "wait", "--qid", "q1", "--timeout", "0"])
        self.assertEqual(rc, 3)
        self.assertIn("no answer to q1", out)

    def test_answer_command_validates_question(self):
        emit("a", "", "started", "x", status_dir=self.d)
        rc, _, err = run(cmd_main, ["answer", "a", "q1", "yes", "--dir", str(self.d)])
        self.assertEqual(rc, 2)
        self.assertIn("no question 'q1'", err)
        ask("a", "", "Proceed?", status_dir=self.d)
        rc, out, _ = run(cmd_main, ["answer", "a", "q1", "yes", "--dir", str(self.d), "--from", "chris"])
        self.assertEqual(rc, 0)
        self.assertIn("queued m-", out)
        rc, out, _ = run(cmd_main, ["questions", "--dir", str(self.d), "--json"])
        self.assertEqual(json.loads(out)[0]["status"], Q_ANSWERED)
        rc, out, _ = run(cmd_main, ["pending", "--dir", str(self.d)])
        self.assertIn("answer q1: yes", out)

    def test_steer_unknown_agent_needs_force(self):
        rc, _, err = run(cmd_main, ["steer", "ghost", "hi", "--dir", str(self.d)])
        self.assertEqual(rc, 2)
        rc, _, _ = run(cmd_main, ["steer", "ghost", "hi", "--dir", str(self.d), "--force"])
        self.assertEqual(rc, 0)
        self.assertNotIn("ghost", [a.name for a in load_monitor(self.d).rows()])  # inbox alone isn't a row

    def test_post_validation(self):
        with self.assertRaises(ValueError):
            inbox.post(self.d, "a", "answer", "no ref")
        with self.assertRaises(ValueError):
            inbox.post(self.d, "a", "steer", "   ")
        with self.assertRaises(ValueError):
            inbox.post(self.d, "a", "shout", "x")
        inbox.post(self.d, "a", "stop", "")  # stop may be empty

    def test_malformed_inbox_lines_counted(self):
        emit("a", "", "started", "x", status_dir=self.d)
        with open(inbox.inbox_path(self.d, "a"), "a", encoding="utf-8") as f:
            f.write("garbage\n" + json.dumps({"kind": "steer", "text": "no id"}) + "\n")
        self.assertEqual(load_monitor(self.d).error_count, 2)

    def test_render_shows_open_questions(self):
        emit("a", "M00-04", "progress", "x", status_dir=self.d)
        ask("a", "M00-04", "Which ROM revision?", status_dir=self.d, blocking=True, default="REV01")
        ask("a", "M00-04", "Nice to have?", status_dir=self.d)
        inbox.post(self.d, "a", "answer", "yes", ref="q2")
        text = "\n".join(render(load_monitor(self.d), datetime.now(timezone.utc), width=110, warn_after=W,
                                stale_after=S))
        for needle in ("OPEN QUESTIONS (2)", "BLOCKING", "Which ROM revision?", "default: REV01",
                       "answered, awaiting pickup", "ASK", "inbox not yet picked up: a (1)"):
            self.assertIn(needle, text)


if __name__ == "__main__":
    unittest.main()
