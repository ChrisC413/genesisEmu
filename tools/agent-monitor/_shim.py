"""Temporary shim: agent-monitor moved to https://github.com/ChrisC413/agent-monitor (genesisEmu D-027).

Finds the real tool via $AGENT_MONITOR, or a clone named ``agent-monitor`` next to this checkout
(e.g. C:/Users/chris/Desktop/projects/agent-monitor), and runs the requested script there with the
same arguments. Remove after the next genesisEmu milestone (agent-monitor story AM2-04).
"""
from __future__ import annotations

import os
import runpy
import sys
from pathlib import Path

REPO = "https://github.com/ChrisC413/agent-monitor"


def find_tool() -> Path | None:
    env = os.environ.get("AGENT_MONITOR")
    if env and (Path(env) / "agent_monitor").is_dir():
        return Path(env)
    here = Path(__file__).resolve()
    for anc in here.parents[1:]:  # skip this shim folder itself
        cand = anc / "agent-monitor"
        if (cand / "agent_monitor").is_dir() and cand != here.parent:
            return cand
    return None


def run(script: str) -> None:
    tool = find_tool()
    if tool is None:
        sys.stderr.write(
            f"agent-monitor not found. Clone {REPO} next to genesisEmu or set AGENT_MONITOR to its path.\n"
            "Fallback: append a v1 line yourself, e.g.\n"
            '  echo \'{"ts":"2026-10-24T12:00:00Z","agent":"<agent>","story":"<id>","state":"progress",'
            '"message":"..."}\' >> <main checkout>/.agent-status/<agent>.jsonl\n')
        sys.exit(2)
    target = tool / script
    sys.argv[0] = str(target)
    runpy.run_path(str(target), run_name="__main__")
