import json
import os
import sys
import unittest
from datetime import datetime, timedelta, timezone

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

from agent_monitor.state import LIVE_IDLE, LIVE_OK, LIVE_QUIET, LIVE_STALE, Monitor  # noqa: E402

T0 = datetime(2026, 10, 24, 11, 0, 0, tzinfo=timezone.utc)
WARN, STALE = timedelta(minutes=5), timedelta(minutes=15)


def line(minute, agent="a", state="progress", story="M00-01", message="msg", **extra):
    d = {"ts": (T0 + timedelta(minutes=minute)).strftime("%Y-%m-%dT%H:%M:%SZ"), "agent": agent,
         "story": story, "state": state, "message": message}
    d.update(extra)
    return json.dumps(d)


class MonitorTests(unittest.TestCase):
    def test_one_row_per_agent_latest_wins(self):
        m = Monitor()
        m.ingest_line(line(0, "a", "started", "M00-01", "start a"))
        m.ingest_line(line(1, "b", "started", "T01-01", "start b"))
        m.ingest_line(line(2, "a", "progress", "M00-01", "half way"))
        self.assertEqual(set(m.agents), {"a", "b"})
        a = m.agents["a"]
        self.assertEqual((a.story, a.state, a.message, a.events), ("M00-01", "progress", "half way", 2))
        self.assertEqual([r.name for r in m.rows()], ["a", "b"])  # most recent first

    def test_story_transitions_and_done_list(self):
        m = Monitor()
        m.ingest_line(line(0, story="M00-01", state="started", percent=10))
        m.ingest_line(line(1, story="M00-01", state="done"))
        m.ingest_line(line(2, story="M00-02", state="started"))
        a = m.agents["a"]
        self.assertEqual((a.story, a.state), ("M00-02", "started"))
        self.assertEqual(a.stories_done, ["M00-01"])
        self.assertNotIn("percent", a.extra)  # percent belongs to the previous story

    def test_malformed_lines_counted_not_fatal(self):
        m = Monitor()
        m.ingest_line("garbage", "x/a.jsonl", 1)
        m.ingest_line('{"ts":"2026-10-24T11:00:00Z","agent":"a","sta', "x/a.jsonl", 2)
        m.ingest_line("", "x/a.jsonl", 3)  # blank: ignored, not an error
        m.ingest_line(line(0), "x/a.jsonl", 4)
        self.assertEqual(m.error_count, 2)
        self.assertEqual(m.errors[-1].lineno, 2)
        self.assertEqual(m.agents["a"].events, 1)

    def test_agent_name_falls_back_to_file_stem(self):
        m = Monitor()
        m.ingest_line('{"ts":"2026-10-24T11:00:00Z","state":"started"}', "dir/m00-foundation.jsonl", 1)
        self.assertIn("m00-foundation", m.agents)

    def test_heartbeat_updates_liveness_only(self):
        m = Monitor()
        m.ingest_line(line(0, state="progress", message="building"))
        m.ingest_line(line(20, state="heartbeat", message=""))
        a = m.agents["a"]
        self.assertEqual((a.state, a.message), ("progress", "building"))
        self.assertEqual(a.liveness(T0 + timedelta(minutes=21), WARN, STALE), LIVE_OK)
        self.assertEqual(len(m.feed), 1)  # heartbeats don't flood the feed

    def test_out_of_order_event_does_not_override(self):
        m = Monitor()
        m.ingest_line(line(5, state="blocked", message="new"))
        m.ingest_line(line(1, state="progress", message="old"))
        self.assertEqual(m.agents["a"].state, "blocked")
        self.assertEqual([e.message for e in m.recent(10)], ["old", "new"])  # feed sorted by ts

    def test_stale_detection(self):
        m = Monitor()
        m.ingest_line(line(0, state="progress"))
        a = m.agents["a"]
        self.assertEqual(a.liveness(T0 + timedelta(minutes=4, seconds=59), WARN, STALE), LIVE_OK)
        self.assertEqual(a.liveness(T0 + timedelta(minutes=5), WARN, STALE), LIVE_QUIET)
        self.assertEqual(a.liveness(T0 + timedelta(minutes=15), WARN, STALE), LIVE_STALE)
        # Clock skew (event from the "future") is treated as age 0, not negative.
        self.assertEqual(a.liveness(T0 - timedelta(minutes=3), WARN, STALE), LIVE_OK)

    def test_blocked_agent_can_go_stale(self):
        m = Monitor()
        m.ingest_line(line(0, state="blocked"))
        self.assertEqual(m.agents["a"].liveness(T0 + timedelta(hours=1), WARN, STALE), LIVE_STALE)

    def test_finished_agent_is_idle_not_stale(self):
        for st in ("done", "failed"):
            m = Monitor()
            m.ingest_line(line(0, state=st))
            self.assertEqual(m.agents["a"].liveness(T0 + timedelta(days=2), WARN, STALE), LIVE_IDLE)

    def test_recent_returns_newest_n_oldest_first(self):
        m = Monitor()
        for i in range(10):
            m.ingest_line(line(i, message=str(i)))
        self.assertEqual([e.message for e in m.recent(3)], ["7", "8", "9"])
        self.assertEqual(m.recent(0), [])


if __name__ == "__main__":
    unittest.main()
