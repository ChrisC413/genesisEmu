import os
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

from agent_monitor.tail import DirTailer, FileTailer  # noqa: E402

L1 = '{"ts":"2026-10-24T11:00:00Z","agent":"a","state":"started","message":"one"}'
L2 = '{"ts":"2026-10-24T11:01:00Z","agent":"a","state":"progress","message":"two"}'


class FakeClock:
    def __init__(self):
        self.t = 0.0

    def __call__(self):
        return self.t


class TailTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.dir = Path(self.tmp.name)
        self.clock = FakeClock()

    def tearDown(self):
        self.tmp.cleanup()

    def write(self, name, data: str | bytes):
        with open(self.dir / name, "ab") as f:
            f.write(data.encode() if isinstance(data, str) else data)

    def texts(self, lines):
        return [ln.text for ln in lines]

    def test_reads_complete_lines_incrementally(self):
        self.write("a.jsonl", L1 + "\n")
        t = FileTailer(self.dir / "a.jsonl", clock=self.clock)
        self.assertEqual(self.texts(t.read_new()), [L1])
        self.assertEqual(t.read_new(), [])
        self.write("a.jsonl", L2 + "\n")
        got = t.read_new()
        self.assertEqual(self.texts(got), [L2])
        self.assertEqual(got[0].lineno, 2)

    def test_partial_last_line_is_buffered_until_newline(self):
        self.write("a.jsonl", L1 + "\n" + L2[:20])
        t = FileTailer(self.dir / "a.jsonl", clock=self.clock)
        self.assertEqual(self.texts(t.read_new()), [L1])
        self.write("a.jsonl", L2[20:40])
        self.assertEqual(t.read_new(), [])
        self.write("a.jsonl", L2[40:] + "\n")
        self.assertEqual(self.texts(t.read_new()), [L2])

    def test_partial_invalid_json_never_flushed(self):
        self.write("a.jsonl", L2[:30])
        t = FileTailer(self.dir / "a.jsonl", partial_grace=1.0, clock=self.clock)
        self.assertEqual(t.read_new(), [])
        self.clock.t = 100
        self.assertEqual(t.read_new(), [])

    def test_unterminated_complete_line_flushed_after_grace_once(self):
        self.write("a.jsonl", L1)
        t = FileTailer(self.dir / "a.jsonl", partial_grace=2.0, clock=self.clock)
        self.assertEqual(t.read_new(), [])
        self.clock.t = 1.0
        self.assertEqual(t.read_new(), [])
        self.clock.t = 2.5
        self.assertEqual(self.texts(t.read_new()), [L1])
        self.clock.t = 10
        self.assertEqual(t.read_new(), [])
        # The writer finally terminates it and appends another line: no duplicate.
        self.write("a.jsonl", "\n" + L2 + "\n")
        got = t.read_new()
        self.assertEqual(self.texts(got), [L2])
        self.assertEqual(got[0].lineno, 2)

    def test_crlf_and_invalid_utf8(self):
        self.write("a.jsonl", (L1 + "\r\n").encode() + b"\xff\xfe\n")
        t = FileTailer(self.dir / "a.jsonl", clock=self.clock)
        got = self.texts(t.read_new())
        self.assertEqual(got[0], L1)
        self.assertEqual(len(got), 2)  # undecodable bytes become a (malformed) line, not a crash

    def test_truncation_restarts(self):
        self.write("a.jsonl", L1 + "\n" + L2 + "\n")
        t = FileTailer(self.dir / "a.jsonl", clock=self.clock)
        self.assertEqual(len(t.read_new()), 2)
        (self.dir / "a.jsonl").write_bytes((L2 + "\n").encode())
        got = t.read_new()
        self.assertEqual((self.texts(got), got[0].lineno), ([L2], 1))

    def test_missing_file_is_quiet(self):
        t = FileTailer(self.dir / "nope.jsonl", clock=self.clock)
        self.assertEqual(t.read_new(), [])

    def test_dir_tailer_picks_up_new_files_and_ignores_others(self):
        d = DirTailer(self.dir, clock=self.clock)
        self.assertEqual(d.poll(), [])
        self.write("a.jsonl", L1 + "\n")
        self.write("notes.txt", "hello\n")
        self.assertEqual(self.texts(d.poll()), [L1])
        self.write("b.jsonl", L2 + "\n")
        got = d.poll()
        self.assertEqual([Path(ln.source).name for ln in got], ["b.jsonl"])

    def test_missing_directory_is_quiet(self):
        d = DirTailer(self.dir / "absent", clock=self.clock)
        self.assertEqual(d.poll(), [])


if __name__ == "__main__":
    unittest.main()
