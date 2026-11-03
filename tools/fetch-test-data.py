#!/usr/bin/env python3
"""Fetch permissively licensed test suites into the git-ignored test-data/ folder (spec 13).

Everything is pinned: the SingleStepTests 680x0 and z80 suites by commit hash, with each file checked against
the git blob SHA-1 recorded in tools/test-data.lock.json; ZEXDOC/ZEXALL and The Macro Assembler AS by URL +
SHA-256.

    python tools/fetch-test-data.py              # everything (68000 ~200 MB, Z80 ~1.4 GB download)
    python tools/fetch-test-data.py --only m68k  # just the 68000 vectors
    python tools/fetch-test-data.py --only z80   # just the Z80 vectors
    python tools/fetch-test-data.py --only zex   # just ZEXDOC / ZEXALL
    python tools/fetch-test-data.py --only as    # just the assembler
    python tools/fetch-test-data.py --update-lock  # re-pin the file lists (needs network; maintainers only)

The CPU vectors are converted to compact binary formats (.sst for the 68000, .ssz for the Z80; documented in
tests/m68k/sst_format.hpp and tests/z80/ssz_format.hpp) that the C++ runners stream without a JSON parser.
Already converted files whose recorded source hash matches the lock are skipped, so re-running is cheap and CI
can cache test-data/m68k and test-data/z80.
"""

from __future__ import annotations

import argparse
import gzip
import hashlib
import io
import json
import os
import struct
import sys
import tempfile
import urllib.request
import zipfile
from urllib.parse import quote
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
LOCK = ROOT / "tools" / "test-data.lock.json"
OUT = ROOT / "test-data"

SST_MAGIC = b"SST68K\x01\x00"
REG_ORDER = ["d0", "d1", "d2", "d3", "d4", "d5", "d6", "d7",
             "a0", "a1", "a2", "a3", "a4", "a5", "a6", "usp", "ssp", "sr", "pc"]


def git_blob_sha1(data: bytes) -> str:
    return hashlib.sha1(b"blob %d\0" % len(data) + data).hexdigest()


def download(url: str) -> bytes:
    req = urllib.request.Request(url, headers={"User-Agent": "genesisEmu-fetch-test-data"})
    with urllib.request.urlopen(req, timeout=120) as resp:
        return resp.read()


def write_state(out: io.BufferedIOBase, state: dict) -> None:
    out.write(struct.pack("<19I", *[state[k] & 0xFFFFFFFF for k in REG_ORDER]))
    out.write(struct.pack("<2H", *state["prefetch"]))
    ram = state["ram"]
    out.write(struct.pack("<I", len(ram)))
    for addr, value in ram:
        out.write(struct.pack("<IB", addr, value))


def convert(raw_gz: bytes, source_sha: str, dest: Path) -> int:
    tests = json.loads(gzip.decompress(raw_gz))
    tmp = dest.with_suffix(".tmp")
    with open(tmp, "wb") as out:
        out.write(SST_MAGIC)
        out.write(source_sha.encode("ascii"))  # 40 bytes
        out.write(struct.pack("<I", len(tests)))
        for t in tests:
            name = t["name"].encode("utf-8")
            out.write(struct.pack("<H", len(name)))
            out.write(name)
            write_state(out, t["initial"])
            write_state(out, t["final"])
            out.write(struct.pack("<I", t["length"]))
    os.replace(tmp, dest)
    return len(tests)


def recorded_sha(path: Path) -> str | None:
    try:
        with open(path, "rb") as f:
            head = f.read(len(SST_MAGIC) + 40)
    except OSError:
        return None
    if len(head) != len(SST_MAGIC) + 40 or not head.startswith(SST_MAGIC):
        return None
    return head[len(SST_MAGIC):].decode("ascii", "replace")


def fetch_m68k(lock: dict, only_files: list[str] | None) -> bool:
    suite = lock["m68k"]
    dest_dir = OUT / "m68k"
    dest_dir.mkdir(parents=True, exist_ok=True)
    files = suite["files"]
    if only_files:
        files = {k: v for k, v in files.items() if Path(k).name.split(".json")[0] in only_files}
    ok = True
    fetched = skipped = total_tests = 0
    for rel, info in sorted(files.items()):
        name = Path(rel).name.removesuffix(".json.gz")
        dest = dest_dir / f"{name}.sst"
        if recorded_sha(dest) == info["sha1"]:
            skipped += 1
            continue
        url = f"https://raw.githubusercontent.com/{suite['repo']}/{suite['commit']}/{rel}"
        raw = download(url)
        actual = git_blob_sha1(raw)
        if actual != info["sha1"] or len(raw) != info["size"]:
            print(f"checksum mismatch for {rel}: got {actual}, expected {info['sha1']}", file=sys.stderr)
            ok = False
            continue
        total_tests += convert(raw, info["sha1"], dest)
        fetched += 1
        print(f"  {name}: ok", flush=True)
    print(f"m68k: {fetched} fetched ({total_tests} tests), {skipped} up to date -> {dest_dir}")
    return ok


SSZ_MAGIC = b"SSTZ80\x01\x00"
Z80_WORDS = ["pc", "sp", "ix", "iy", "wz", "af_", "bc_", "de_", "hl_"]
Z80_BYTES = ["a", "f", "b", "c", "d", "e", "h", "l", "i", "r", "im", "iff1", "iff2", "ei", "p", "q"]


def write_z80_state(out: io.BufferedIOBase, state: dict) -> None:
    out.write(struct.pack("<9H", *[state[k] & 0xFFFF for k in Z80_WORDS]))
    out.write(struct.pack("<16B", *[state[k] & 0xFF for k in Z80_BYTES]))
    ram = state["ram"]
    out.write(struct.pack("<H", len(ram)))
    for addr, value in ram:
        out.write(struct.pack("<HB", addr, value))


def convert_z80(raw: bytes, source_sha: str, dest: Path) -> int:
    tests = json.loads(raw)
    tmp = dest.with_suffix(".tmp")
    with open(tmp, "wb") as out:
        out.write(SSZ_MAGIC)
        out.write(source_sha.encode("ascii"))  # 40 bytes
        out.write(struct.pack("<I", len(tests)))
        for t in tests:
            name = t["name"].encode("utf-8")
            out.write(struct.pack("<H", len(name)))
            out.write(name)
            write_z80_state(out, t["initial"])
            write_z80_state(out, t["final"])
            ports = t.get("ports", [])
            out.write(struct.pack("<HH", len(t["cycles"]), len(ports)))
            for addr, value, kind in ports:
                out.write(struct.pack("<HBB", addr, value, 1 if kind == "w" else 0))
    os.replace(tmp, dest)
    return len(tests)


def recorded_z80_sha(path: Path) -> str | None:
    try:
        with open(path, "rb") as f:
            head = f.read(len(SSZ_MAGIC) + 40)
    except OSError:
        return None
    if len(head) != len(SSZ_MAGIC) + 40 or not head.startswith(SSZ_MAGIC):
        return None
    return head[len(SSZ_MAGIC):].decode("ascii", "replace")


def fetch_z80(lock: dict) -> bool:
    suite = lock["z80"]
    dest_dir = OUT / "z80"
    dest_dir.mkdir(parents=True, exist_ok=True)
    ok = True
    fetched = skipped = 0
    for rel, info in sorted(suite["files"].items()):
        name = Path(rel).name.removesuffix(".json")
        dest = dest_dir / f"{name}.ssz"
        if recorded_z80_sha(dest) == info["sha1"]:
            skipped += 1
            continue
        url = f"https://raw.githubusercontent.com/{suite['repo']}/{suite['commit']}/{quote(rel)}"
        raw = download(url)
        actual = git_blob_sha1(raw)
        if actual != info["sha1"] or len(raw) != info["size"]:
            print(f"checksum mismatch for {rel}: got {actual}, expected {info['sha1']}", file=sys.stderr)
            ok = False
            continue
        convert_z80(raw, info["sha1"], dest)
        fetched += 1
        if fetched % 100 == 0:
            print(f"  z80: {fetched} files converted", flush=True)
    print(f"z80: {fetched} fetched, {skipped} up to date -> {dest_dir}")
    return ok


def fetch_zex(lock: dict) -> bool:
    info = lock["zex"]
    dest = OUT / "zex"
    dest.mkdir(parents=True, exist_ok=True)
    ok = True
    for name, file in info["files"].items():
        target = dest / name
        if target.exists() and hashlib.sha256(target.read_bytes()).hexdigest() == file["sha256"]:
            continue
        data = download(f"https://raw.githubusercontent.com/{info['repo']}/{info['commit']}/{name}")
        actual = hashlib.sha256(data).hexdigest()
        if actual != file["sha256"]:
            print(f"zex: checksum mismatch for {name}: got {actual}, expected {file['sha256']}",
                  file=sys.stderr)
            ok = False
            continue
        target.write_bytes(data)
    print(f"zex: {', '.join(info['files'])} -> {dest}")
    return ok


def fetch_as(lock: dict) -> bool:
    info = lock.get("as")
    if not info:
        print("as: no pinned build in the lock file")
        return True
    key = {"win32": "windows"}.get(sys.platform, sys.platform)
    build = info["builds"].get(key)
    if build is None:
        print(f"as: no pinned binary for {sys.platform}; build AS from source and set GENESIS_ASL "
              "(see tools/test-data.lock.json)")
        return True
    dest = OUT / "as" / key
    stamp = dest / ".sha256"
    if stamp.exists() and stamp.read_text().strip() == build["sha256"]:
        print(f"as: up to date -> {dest}")
        return True
    data = download(build["url"])
    actual = hashlib.sha256(data).hexdigest()
    if actual != build["sha256"]:
        print(f"as: checksum mismatch: got {actual}, expected {build['sha256']}", file=sys.stderr)
        return False
    dest.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(io.BytesIO(data)) as zf:
        for member in zf.infolist():
            target = (dest / member.filename).resolve()
            if not str(target).startswith(str(dest.resolve())):
                raise RuntimeError(f"unsafe path in archive: {member.filename}")
        zf.extractall(dest)
    stamp.write_text(build["sha256"])
    print(f"as: {build['version']} -> {dest}")
    return True


def update_lock(lock: dict) -> None:
    suite = lock["m68k"]
    api = f"https://api.github.com/repos/{suite['repo']}/git/trees/{suite['commit']}?recursive=1"
    tree = json.loads(download(api))
    files = {}
    for entry in tree["tree"]:
        if entry["type"] == "blob" and entry["path"].startswith(suite["path"] + "/") and \
                entry["path"].endswith(".json.gz"):
            files[entry["path"]] = {"sha1": entry["sha"], "size": entry["size"]}
    suite["files"] = dict(sorted(files.items()))
    print(f"lock m68k: {len(files)} files, {sum(f['size'] for f in files.values())} bytes")

    z80 = lock["z80"]
    api = f"https://api.github.com/repos/{z80['repo']}/git/trees/{z80['commit']}?recursive=1"
    tree = json.loads(download(api))
    files = {}
    for entry in tree["tree"]:
        if entry["type"] == "blob" and entry["path"].startswith(z80["path"] + "/") and \
                entry["path"].endswith(".json"):
            files[entry["path"]] = {"sha1": entry["sha"], "size": entry["size"]}
    z80["files"] = dict(sorted(files.items()))
    print(f"lock z80: {len(files)} files, {sum(f['size'] for f in files.values())} bytes")

    zex = lock["zex"]
    for name, file in zex["files"].items():
        data = download(f"https://raw.githubusercontent.com/{zex['repo']}/{zex['commit']}/{name}")
        file["sha256"] = hashlib.sha256(data).hexdigest()
        file["size"] = len(data)
    LOCK.write_text(json.dumps(lock, indent=2) + "\n", newline="\n")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--only", choices=["m68k", "z80", "zex", "as"], help="fetch only one suite")
    parser.add_argument("--files", nargs="*", help="m68k: only these files (e.g. NOP MOVE.w)")
    parser.add_argument("--update-lock", action="store_true", help="re-pin the m68k file list from GitHub")
    args = parser.parse_args()

    lock = json.loads(LOCK.read_text())
    if args.update_lock:
        update_lock(lock)
        return 0
    ok = True
    if args.only in (None, "m68k"):
        ok &= fetch_m68k(lock, args.files)
    if args.only in (None, "z80"):
        ok &= fetch_z80(lock)
    if args.only in (None, "zex"):
        ok &= fetch_zex(lock)
    if args.only in (None, "as"):
        ok &= fetch_as(lock)
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
