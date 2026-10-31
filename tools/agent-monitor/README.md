# agent-monitor (moved)

agent-monitor now lives in its own private repository:
**https://github.com/ChrisC413/agent-monitor**. Its specs, stories and tests are there.

This folder holds a temporary **shim**. `emit.py` and `monitor.py` here forward to the real tool,
which is found in one of two places:

- the path in `$AGENT_MONITOR`, or
- a clone named `agent-monitor` next to this checkout, e.g. `C:\Users\chris\Desktop\projects\agent-monitor`.

Prompts and worktrees that call `tools/agent-monitor/emit.py` keep working this way. The shim will be
removed after the next genesisEmu milestone. See [spec 17](../../docs/specs/17-agent-monitoring.md).
