# T01-05 · Attention signals and notifications

**Milestone:** [T01 – Agent monitoring](README.md)  
**Status:** Draft  
**Depends on:** T01-02, T01-03  
**Spec refs:** [17-agent-monitoring](../../docs/specs/17-agent-monitoring.md#open-questions) (MQ3)

## Story

As a **developer**, I want to be told when an agent becomes blocked, fails, goes stale or breaks CI, so that I don't have to keep watching the screen.

## Scope

- Transition detection (only on change, not on every redraw)
- Terminal bell; opt-in `--notify` OS notifications (Windows toast via PowerShell, macOS `osascript`)
- Attention summary line at the top of the view

## Acceptance criteria

- [ ] Each transition notifies exactly once
- [ ] Notifications are off unless `--notify` is given
- [ ] Works without notification support (falls back to bell)

## Test plan

- Unit tests for transition detection with a fake clock

## Notes

—
