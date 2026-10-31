"""Fail if any tracked text file contains a carriage-return byte.

.gitattributes stores text files with LF endings, so a CR in the repository is always a mistake —
for example a raw CR inside a C++ character literal, which MSVC accepts but clang rejects.
Windows CI can't catch that by compiling, so this check runs everywhere.
"""

import subprocess
import sys

TEXT_SUFFIXES = (".c", ".cpp", ".h", ".hpp", ".inl", ".py", ".sh", ".cmake", ".txt", ".json", ".yml", ".yaml", ".md")
# .gitattributes deliberately keeps CRLF for these Windows scripts.
CRLF_ALLOWED = (".bat", ".ps1")


def main() -> int:
    files = subprocess.run(["git", "ls-files", "-z"], check=True, capture_output=True).stdout.decode().split("\0")
    bad = []
    for path in files:
        if not path.endswith(TEXT_SUFFIXES) or path.endswith(CRLF_ALLOWED):
            continue
        with open(path, "rb") as f:
            for lineno, line in enumerate(f.read().split(b"\n"), 1):
                if b"\r" in line:
                    bad.append(f"{path}:{lineno}")
    for entry in bad:
        print(f"carriage return in {entry}")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
