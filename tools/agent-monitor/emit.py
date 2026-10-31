#!/usr/bin/env python3
"""Shim: runs emit.py from the agent-monitor repository (see _shim.py and docs/specs/17-agent-monitoring.md)."""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from _shim import run  # noqa: E402

run("emit.py")
