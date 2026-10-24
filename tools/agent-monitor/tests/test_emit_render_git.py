import os
import sys
import tempfile
import threading
import unittest
from datetime import datetime, timedelta, timezone
from pathlib import Path

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

from agent_monitor.emit import emit, make_event  # noqa: E402
from agent_monitor.gitinfo import AgentGit, parse_worktree_porcelain, summarize_checks  # noqa: E402
from agent_monitor.protocol import parse_line  # noqa: E402
from agent_monitor.render import fmt_age, render  # noqa: E402
from agent_monitor.state import Monitor  # noqa: E402
from agent_monitor.tail import DirTailer  # noqa: E402


class EmitTests(unittest.TestCase):
    def test_emitted_line_parses_back(self):
        with tempfile.TemporaryDirectory() as d:
            emit("m00-foundation", "M00-04", "progress", "two\nlines", status_dir=Path(d), percent=50)
            text = (Path(d) / "m00-foundation.jsonl").read_text(encoding="utf-8")
            self.assertTrue(text.endswith("\n"))
            self.assertEqual(text.count("\n"), 1)
            ev = parse_line(text)
            self.assertEqual((ev.story, ev.state, ev.message, ev.extra), ("M00-04", "progress", "two lines",
                                                                       {"percent": 50}))
            self.assertTrue(text.startswith('{"ts":"') and '"Z"' not in text and 'Z","agent"' in text)

    def test_rejects_bad_input(self):
        with self.assertRaises(ValueError):
            make_event("a", "s", "finished", "")
        with self.assertRaises(ValueError):
            make_event("bad/name", "s", "done", "")

    def test_none_extras_dropped_and_core_fields_protected(self):
        ev = make_event("a", "s", "done", "m", percent=None, branch="x")
        self.assertEqual(set(ev), {"ts", "agent", "story", "state", "message", "branch"})

    def test_concurrent_writers_never_interleave(self):
        with tempfile.TemporaryDirectory() as d:
            def worker(i):
                for j in range(50):
                    emit("a", "T01-01", "progress", f"w{i} n{j} " + "x" * 200, status_dir=Path(d))
            threads = [threading.Thread(target=worker, args=(i,)) for i in range(4)]
            for t in threads:
                t.start()
            for t in threads:
                t.join()
            m = Monitor()
            for ln in DirTailer(Path(d)).poll():
                m.ingest_line(ln.text, ln.source, ln.lineno)
            self.assertEqual((m.error_count, m.agents["a"].events), (0, 200))


class RenderTests(unittest.TestCase):
    def test_snapshot_contents_and_ascii(self):
        t0 = datetime(2026, 10, 24, 11, 0, tzinfo=timezone.utc)
        m = Monitor()
        m.ingest_line(f'{{"ts":"{t0:%Y-%m-%dT%H:%M:%SZ}","agent":"m00","story":"M00-04",'
                      '"state":"blocked","message":"needs ROM \u2014 none found","percent":30}')
        m.ingest_line("broken", "d/m00.jsonl", 2)
        out = render(m, t0 + timedelta(minutes=20), width=100, warn_after=timedelta(minutes=5),
                     stale_after=timedelta(minutes=15),
                     git={"m00": AgentGit(branch="m00", commit="abc123 Init (2 hours ago)",
                                          pr="#3 draft open", checks="fail")})
        text = "\n".join(out)
        text.encode("ascii")  # must not raise
        for needle in ("m00", "M00-04", "blocked", "STALE", "[ 30%]", "20m00s", "PR #3 draft open",
                       "CI fail", "MALFORMED LINES: 1", "m00.jsonl:2"):
            self.assertIn(needle, text)
        self.assertTrue(all(len(l) <= 100 for l in out))

    def test_fmt_age(self):
        self.assertEqual([fmt_age(timedelta(seconds=s)) for s in (5, 65, 3700, 90000)],
                         ["5s", "1m05s", "1h01m", "1d01h"])
        self.assertEqual(fmt_age(None), "-")


class GitParsingTests(unittest.TestCase):
    def test_worktree_porcelain(self):
        text = ("worktree C:/r\nHEAD 5da83ab\nbranch refs/heads/main\n\n"
                "worktree C:/r/.claude/worktrees/x\nHEAD 5da83ab\nbranch refs/heads/m00-foundation\nlocked\n\n"
                "worktree C:/r/w2\nHEAD abc\ndetached\n")
        trees = parse_worktree_porcelain(text)
        self.assertEqual([(t.path, t.branch) for t in trees],
                         [("C:/r", "main"), ("C:/r/.claude/worktrees/x", "m00-foundation"), ("C:/r/w2", "")])

    def test_summarize_checks(self):
        ok = {"__typename": "CheckRun", "status": "COMPLETED", "conclusion": "SUCCESS"}
        skip = {"__typename": "CheckRun", "status": "COMPLETED", "conclusion": "SKIPPED"}
        run = {"__typename": "CheckRun", "status": "IN_PROGRESS", "conclusion": ""}
        bad = {"__typename": "CheckRun", "status": "COMPLETED", "conclusion": "FAILURE"}
        ctx_pending = {"__typename": "StatusContext", "state": "PENDING"}
        self.assertEqual(summarize_checks(None), "none")
        self.assertEqual(summarize_checks([ok, skip]), "pass")
        self.assertEqual(summarize_checks([ok, run]), "pending")
        self.assertEqual(summarize_checks([ok, ctx_pending]), "pending")
        self.assertEqual(summarize_checks([run, bad]), "fail")


if __name__ == "__main__":
    unittest.main()
