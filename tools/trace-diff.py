#!/usr/bin/env python3
"""Compare two genesisEmu text traces and report the first divergence (spec 13, M02-06).

Both files use the trace format of docs/specs/08-debugger.md ("# genesisEmu trace v1"). A trace from a
reference emulator must be converted to that format first. By default the timing columns (frame, line,
mclk) are ignored, because reference emulators count time differently; everything from the CPU column on
(PC, opcode words, disassembly, register deltas, memory writes) must match.

Exit codes: 0 identical, 1 divergence found, 2 usage / read error.
"""

from __future__ import annotations

import argparse
import itertools
import sys
from pathlib import Path


def records(path: Path):
    """Yields (line number, full text) for every record line."""
    with path.open("r", encoding="utf-8", errors="replace") as f:
        for number, line in enumerate(f, 1):
            line = line.rstrip("\r\n")
            if line and not line.startswith("#"):
                yield number, line


def key(line: str, timing: bool, writes: bool, disasm: bool) -> str:
    """The part of a record that is compared."""
    head, sep, rest = line.partition(" M68K ")
    if not sep:
        head, sep, rest = line.partition(" Z80 ")
    parts = [p.strip() for p in rest.split("|")]
    body = " ".join(parts[0].split())
    if not disasm:
        # Keep the PC and opcode words; the words column is 4-digit hex groups.
        fields = body.split()
        kept = [fields[0]] if fields else []
        for f in fields[1:]:
            if len(f) == 4 and all(c in "0123456789ABCDEF" for c in f):
                kept.append(f)
            else:
                break
        body = " ".join(kept)
    deltas = parts[1] if len(parts) > 1 else ""
    mem = parts[2] if len(parts) > 2 and writes else ""
    prefix = " ".join(head.split()) + " " if timing else ""
    return f"{prefix}{sep.strip()} {body} | {deltas} | {mem}"


def main(argv: list[str]) -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("ours", type=Path, help="trace from genesisEmu")
    ap.add_argument("reference", type=Path, help="reference trace (same format)")
    ap.add_argument("--context", type=int, default=5, help="matching records shown before the divergence")
    ap.add_argument("--timing", action="store_true", help="also compare frame, line and mclk")
    ap.add_argument("--no-writes", action="store_true", help="ignore the memory-write column")
    ap.add_argument("--no-disasm", action="store_true", help="ignore the disassembly text (PC and words only)")
    args = ap.parse_args(argv)

    for p in (args.ours, args.reference):
        if not p.is_file():
            print(f"trace-diff: cannot read {p}", file=sys.stderr)
            return 2

    history: list[tuple[int, str]] = []
    count = 0
    a_iter = records(args.ours)
    b_iter = records(args.reference)
    for a, b in itertools.zip_longest(a_iter, b_iter):
        if a is None or b is None:
            longer, which = (b, "reference") if a is None else (a, "ours")
            print(f"traces agree for {count} records; then only {which} continues:")
            print(f"  line {longer[0]}: {longer[1]}")
            return 1
        ka = key(a[1], args.timing, not args.no_writes, not args.no_disasm)
        kb = key(b[1], args.timing, not args.no_writes, not args.no_disasm)
        if ka != kb:
            print(f"first divergence after {count} matching records")
            for n, text in history:
                print(f"    {n:>8}: {text}")
            print(f"  ours {a[0]:>8}: {a[1]}")
            print(f"  ref  {b[0]:>8}: {b[1]}")
            return 1
        count += 1
        history.append(a)
        if len(history) > args.context:
            history.pop(0)
    print(f"traces identical ({count} records)")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
