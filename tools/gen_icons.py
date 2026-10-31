#!/usr/bin/env python3
"""Generate the application icons (a gold ring on a dark rounded square).

Writes src/app/resources/genesisEmu.ico (Windows) and genesisEmu.icns (macOS) using only the
standard library. Re-run after changing the design; the outputs are committed.
"""

import math
import struct
import zlib
from pathlib import Path

OUT = Path(__file__).resolve().parent.parent / "src" / "app" / "resources"


def render(size: int) -> bytes:
    """Return RGBA pixels for a size x size icon, 4x supersampled."""
    ss = 4
    px = bytearray()
    radius = size * 0.18  # corner radius of the background square
    for y in range(size):
        for x in range(size):
            acc = [0.0, 0.0, 0.0, 0.0]
            for sy in range(ss):
                for sx in range(ss):
                    fx = (x + (sx + 0.5) / ss) / size
                    fy = (y + (sy + 0.5) / ss) / size
                    r, g, b, a = sample(fx, fy, radius / size)
                    acc[0] += r * a
                    acc[1] += g * a
                    acc[2] += b * a
                    acc[3] += a
            n = ss * ss
            alpha = acc[3] / n
            if alpha > 0:
                px += bytes(round(c / acc[3] * 255) for c in acc[:3]) + bytes([round(alpha * 255)])
            else:
                px += b"\0\0\0\0"
    return bytes(px)


def sample(x: float, y: float, corner: float):
    # Rounded-square background.
    dx = max(abs(x - 0.5) - (0.5 - corner), 0.0)
    dy = max(abs(y - 0.5) - (0.5 - corner), 0.0)
    if dx * dx + dy * dy > corner * corner:
        return 0.0, 0.0, 0.0, 0.0
    bg = (0.08, 0.12, 0.30)
    # Gold ring with a simple vertical shading.
    d = math.hypot(x - 0.5, y - 0.5)
    if 0.24 <= d <= 0.36:
        shade = 0.75 + 0.25 * (1.0 - y)
        return 1.0 * shade, 0.80 * shade, 0.15 * shade, 1.0
    return (*bg, 1.0)


def png(size: int) -> bytes:
    raw = render(size)
    rows = b"".join(b"\0" + raw[y * size * 4:(y + 1) * size * 4] for y in range(size))

    def chunk(tag: bytes, data: bytes) -> bytes:
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data))

    ihdr = struct.pack(">IIBBBBB", size, size, 8, 6, 0, 0, 0)
    return b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr) + chunk(b"IDAT", zlib.compress(rows, 9)) + chunk(b"IEND", b"")


def write_ico(path: Path, sizes):
    images = [png(s) for s in sizes]
    header = struct.pack("<HHH", 0, 1, len(images))
    offset = 6 + 16 * len(images)
    entries = b""
    for s, data in zip(sizes, images):
        dim = 0 if s >= 256 else s
        entries += struct.pack("<BBBBHHII", dim, dim, 0, 0, 1, 32, len(data), offset)
        offset += len(data)
    path.write_bytes(header + entries + b"".join(images))


def write_icns(path: Path):
    entries = b""
    for tag, size in ((b"icp4", 16), (b"icp5", 32), (b"ic07", 128), (b"ic08", 256), (b"ic09", 512)):
        data = png(size)
        entries += tag + struct.pack(">I", len(data) + 8) + data
    path.write_bytes(b"icns" + struct.pack(">I", len(entries) + 8) + entries)


if __name__ == "__main__":
    OUT.mkdir(parents=True, exist_ok=True)
    write_ico(OUT / "genesisEmu.ico", [16, 24, 32, 48, 256])
    write_icns(OUT / "genesisEmu.icns")
    print(f"wrote icons to {OUT}")
