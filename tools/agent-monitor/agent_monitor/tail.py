"""Incremental, polling tail of ``*.jsonl`` files (no OS-specific file watching).

Polling keeps this identical on Windows and macOS and is cheap at a 1 s interval for a
handful of small files. Only complete lines (ending in ``\\n``) are returned; a partial
trailing line is buffered until the writer finishes it. If a partial line stays
unchanged for ``partial_grace`` seconds *and* is a complete JSON object, it is accepted
anyway (a producer that forgot the final newline must not be invisible forever).
"""

from __future__ import annotations

import json
import os
import time
from dataclasses import dataclass
from pathlib import Path


@dataclass
class Line:
    source: str      # file path
    lineno: int      # 1-based line number in the file
    text: str


class FileTailer:
    def __init__(self, path: Path, partial_grace: float = 3.0, clock=time.monotonic):
        self.path = Path(path)
        self.offset = 0
        self.lineno = 0
        self._partial = b""
        self._partial_since: float | None = None
        self._flushed_partial = False
        self.partial_grace = partial_grace
        self._clock = clock

    def read_new(self) -> list[Line]:
        try:
            size = self.path.stat().st_size
        except OSError:
            return []
        if size < self.offset:  # truncated / rotated: start over
            self.offset, self.lineno = 0, 0
            self._partial, self._partial_since, self._flushed_partial = b"", None, False
        out: list[Line] = []
        if size > self.offset:
            try:
                with open(self.path, "rb") as f:
                    f.seek(self.offset)
                    chunk = f.read(size - self.offset)
            except OSError:
                return []
            self.offset += len(chunk)
            data = self._partial + chunk
            *complete, rest = data.split(b"\n")
            if complete and self._flushed_partial:
                # The partial we already emitted has now been terminated: drop it so
                # it isn't emitted twice (anything appended to it is ignored).
                complete = complete[1:]
                self._flushed_partial = False
            for raw in complete:
                self.lineno += 1
                out.append(Line(str(self.path), self.lineno, raw.decode("utf-8", "replace").rstrip("\r")))
            if rest != self._partial or complete:
                self._partial_since = self._clock() if rest else None
            self._partial = rest
        out.extend(self._maybe_flush_partial())
        return out

    def _maybe_flush_partial(self) -> list[Line]:
        if not self._partial or self._flushed_partial or self._partial_since is None:
            return []
        if self._clock() - self._partial_since < self.partial_grace:
            return []
        if not self._looks_like_json(self._partial):
            return []
        self.lineno += 1
        self._flushed_partial = True
        return [Line(str(self.path), self.lineno, self._partial.decode("utf-8", "replace").rstrip("\r"))]

    @staticmethod
    def _looks_like_json(raw: bytes) -> bool:
        try:
            return isinstance(json.loads(raw.decode("utf-8", "replace")), dict)
        except ValueError:
            return False


class DirTailer:
    """Tails every ``*.jsonl`` in a directory, picking up new files as they appear."""

    def __init__(self, directory: Path, pattern: str = "*.jsonl", partial_grace: float = 3.0,
                 clock=time.monotonic):
        self.directory = Path(directory)
        self.pattern = pattern
        self.partial_grace = partial_grace
        self._clock = clock
        self.tailers: dict[str, FileTailer] = {}

    def poll(self) -> list[Line]:
        if self.directory.is_dir():
            for p in sorted(self.directory.glob(self.pattern)):
                key = os.path.normcase(str(p))
                if key not in self.tailers:
                    self.tailers[key] = FileTailer(p, self.partial_grace, self._clock)
        out: list[Line] = []
        for t in self.tailers.values():
            out.extend(t.read_new())
        return out
