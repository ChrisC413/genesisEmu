#!/usr/bin/env python3
"""Live agent monitor; see README.md. Runs from any directory and any worktree."""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from agent_monitor.cli import main  # noqa: E402

if __name__ == "__main__":
    sys.exit(main())
