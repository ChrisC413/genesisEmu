# agent-monitor

Watch Claude Code agents working on this repo **live**, without interrupting them. Agents append
one-line JSON events to `.agent-status/<agent>.jsonl` in the main checkout; this tool tails those files
and shows one row per agent plus a feed of recent events. Spec: [17-agent-monitoring](../../docs/specs/17-agent-monitoring.md).

Python 3.9+ standard library only (developed on 3.13; 3.9 is what macOS 13's Command Line Tools ship). Windows 11 and macOS 13+. `git` and `gh` are
optional; without them the branch/PR/CI line is simply omitted.

## Watch

```sh
python tools/agent-monitor/monitor.py            # live view, refreshes every second, Ctrl+C to quit
python tools/agent-monitor/monitor.py --once     # one plain-text snapshot (works when piped)
python tools/agent-monitor/monitor.py --json     # one machine-readable snapshot
```

Useful options: `--stale-minutes 15`, `--warn-minutes 5`, `--feed 15`, `--no-gh` (skip PR/CI lookups),
`--no-git`, `--dir <path>`, `--color never`. The status directory defaults to `$AGENT_STATUS_DIR`, else
`<main checkout>/.agent-status`, found through `git rev-parse --git-common-dir`, so it works from any worktree.

`LIVE` column: `QUIET` = no event for `--warn-minutes`, `STALE` = none for `--stale-minutes` (the agent may
be stuck, waiting on a long build, or dead). Agents whose last state is `done`/`failed` are never stale.

## Report (for agents)

```sh
python tools/agent-monitor/emit.py -a m00-foundation -s M00-04 started  "ROM header parser"
python tools/agent-monitor/emit.py -a m00-foundation -s M00-04 progress "Checksum + region decoded" -p 60
python tools/agent-monitor/emit.py -a m00-foundation -s M00-04 heartbeat     # long build running
python tools/agent-monitor/emit.py -a m00-foundation -s M00-04 blocked  "No ROM in GENESIS_ROM_DIR"
python tools/agent-monitor/emit.py -a m00-foundation -s M00-04 done     "12 header tests pass"
```

`AGENT_NAME` / `AGENT_STORY` environment variables can replace `-a` / `-s`. The helper adds `branch` and
`worktree` automatically (disable with `--no-git`), keeps the message on one line, and appends atomically.
Name the agent after its branch (`m00-foundation`) so the monitor can find its commits and PR.

A worktree created before this tool was merged doesn't contain `emit.py`; call the copy in another
checkout by absolute path, or append the line yourself — any writer that produces the v1 format works:

```sh
echo '{"ts":"2026-10-24T11:22:33Z","agent":"m00-foundation","story":"M00-04","state":"progress","message":"..."}' >> .agent-status/m00-foundation.jsonl
```

From Python: `from agent_monitor.emit import emit; emit("m00-foundation", "M00-04", "progress", "...", percent=60)`.

## Test

```sh
python -m unittest discover -s tools/agent-monitor/tests -v
```

## Layout

| File | Role |
|------|------|
| `agent_monitor/protocol.py` | Event v1 + extensions, line parser |
| `agent_monitor/tail.py` | Polling tail of `*.jsonl`, partial-line buffering, truncation handling |
| `agent_monitor/state.py` | Per-agent state, liveness (ok / quiet / stale / idle), event feed |
| `agent_monitor/gitinfo.py` | Optional branch, last commit, PR and CI status via `git` / `gh` (cached, async in live mode) |
| `agent_monitor/render.py` | ASCII table + feed renderer |
| `agent_monitor/cli.py` | `monitor.py` entry point (live, `--once`, `--json`) |
| `agent_monitor/emit.py` | `emit.py` entry point and library helper |

<!-- cspell:words worktree worktrees gitignored stdlib argparse unittest ctypes lockfile npm osascript jsonl -->
