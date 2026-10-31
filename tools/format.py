#!/usr/bin/env python3
"""Format (or with --check, verify) all C/C++ sources tracked by git with clang-format.

CI and developers must use the same clang-format version, pinned below:
    pip install clang-format==22.1.8
Set CLANG_FORMAT to use a specific binary.
"""

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path

PINNED_VERSION = "22.1.8"
PATTERNS = ["*.c", "*.cpp", "*.h", "*.hpp"]
ROOT = Path(__file__).resolve().parent.parent


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--check", action="store_true", help="fail if any file needs formatting")
    args = parser.parse_args()

    exe = os.environ.get("CLANG_FORMAT") or shutil.which("clang-format")
    if not exe:
        print(f"clang-format not found; install it with: pip install clang-format=={PINNED_VERSION}", file=sys.stderr)
        return 2
    version = subprocess.run([exe, "--version"], capture_output=True, text=True, check=True).stdout.strip()
    if PINNED_VERSION not in version:
        print(f"warning: expected clang-format {PINNED_VERSION}, found: {version}", file=sys.stderr)

    files = subprocess.run(["git", "ls-files", "--", *PATTERNS], cwd=ROOT, capture_output=True, text=True,
                           check=True).stdout.split()
    if not files:
        return 0
    cmd = [exe, "--dry-run", "--Werror"] if args.check else [exe, "-i"]
    result = subprocess.run(cmd + files, cwd=ROOT)
    if args.check and result.returncode != 0:
        print("\nFormatting check failed. Run: python tools/format.py", file=sys.stderr)
    return result.returncode


if __name__ == "__main__":
    sys.exit(main())
