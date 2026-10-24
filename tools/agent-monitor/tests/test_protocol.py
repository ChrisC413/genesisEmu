import os
import sys
import unittest
from datetime import datetime, timezone

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

from agent_monitor.protocol import ParseError, parse_line  # noqa: E402

V1 = ('{"ts":"2026-10-24T11:22:33Z","agent":"m00-foundation","story":"M00-04",'
      '"state":"progress","message":"Parsing header"}')


class ParseLineTests(unittest.TestCase):
    def test_v1_line(self):
        ev = parse_line(V1)
        self.assertEqual(ev.agent, "m00-foundation")
        self.assertEqual(ev.story, "M00-04")
        self.assertEqual(ev.state, "progress")
        self.assertEqual(ev.message, "Parsing header")
        self.assertEqual(ev.ts, datetime(2026, 10, 24, 11, 22, 33, tzinfo=timezone.utc))
        self.assertEqual(ev.extra, {})

    def test_roundtrip(self):
        self.assertEqual(parse_line(V1).to_dict()["ts"], "2026-10-24T11:22:33Z")

    def test_extensions_preserved(self):
        ev = parse_line('{"ts":"2026-10-24T11:22:33Z","agent":"a","state":"progress",'
                        '"percent":40,"branch":"m00","future_field":[1]}')
        self.assertEqual(ev.extra, {"percent": 40, "branch": "m00", "future_field": [1]})

    def test_offsets_and_fractions_normalized_to_utc(self):
        ev = parse_line('{"ts":"2026-10-24T07:22:33.250-04:00","agent":"a","state":"done"}')
        self.assertEqual(ev.ts, datetime(2026, 10, 24, 11, 22, 33, 250000, tzinfo=timezone.utc))

    def test_naive_timestamp_is_utc(self):
        ev = parse_line('{"ts":"2026-10-24T11:22:33","agent":"a","state":"done"}')
        self.assertEqual(ev.ts.tzinfo, timezone.utc)

    def test_optional_story_and_message(self):
        ev = parse_line('{"ts":"2026-10-24T11:22:33Z","agent":"a","state":"heartbeat"}')
        self.assertEqual((ev.story, ev.message), ("", ""))

    def test_multiline_message_collapsed(self):
        ev = parse_line('{"ts":"2026-10-24T11:22:33Z","agent":"a","state":"blocked",'
                        '"message":"line one\\nline two"}')
        self.assertEqual(ev.message, "line one line two")

    def test_agent_defaults_to_file_stem(self):
        ev = parse_line('{"ts":"2026-10-24T11:22:33Z","state":"started"}', default_agent="x")
        self.assertEqual(ev.agent, "x")

    def test_unknown_state_is_kept_but_flagged(self):
        ev = parse_line('{"ts":"2026-10-24T11:22:33Z","agent":"a","state":"Paused"}')
        self.assertEqual(ev.state, "paused")
        self.assertFalse(ev.known_state)

    def test_bom_tolerated(self):
        self.assertEqual(parse_line("﻿" + V1).agent, "m00-foundation")

    def test_malformed(self):
        cases = {
            "": "empty",
            "not json": "invalid JSON",
            '{"ts":"2026-10-24T11:22:33Z","agent":"a","state":"pro': "invalid JSON",  # partial
            "[1,2]": "not a JSON object",
            '{"agent":"a","state":"done"}': "missing 'ts'",
            '{"ts":"yesterday","agent":"a","state":"done"}': "bad 'ts'",
            '{"ts":12,"agent":"a","state":"done"}': "bad 'ts'",
            '{"ts":"2026-10-24T11:22:33Z","state":"done"}': "missing 'agent'",
            '{"ts":"2026-10-24T11:22:33Z","agent":"a"}': "missing 'state'",
        }
        for line, reason in cases.items():
            with self.subTest(line=line):
                with self.assertRaises(ParseError) as cm:
                    parse_line(line, source="f.jsonl", lineno=3)
                self.assertIn(reason, cm.exception.reason)
                self.assertEqual(cm.exception.lineno, 3)


if __name__ == "__main__":
    unittest.main()
