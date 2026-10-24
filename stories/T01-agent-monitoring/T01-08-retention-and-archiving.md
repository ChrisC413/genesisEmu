# T01-08 · Retention, filtering and archiving

**Milestone:** [T01 – Agent monitoring](README.md)  
**Status:** Draft  
**Depends on:** T01-02  
**Spec refs:** [17-agent-monitoring](../../docs/specs/17-agent-monitoring.md#open-questions) (MQ5)

## Story

As a **developer**, I want old agents hidden and finished logs archived, so that the view stays focused on current work.

## Scope

- `--since 7d` (default) hides agents with no recent events
- `archive` subcommand moves files of agents whose worktree no longer exists to `.agent-status/archive/`
- Large-file guard: start tailing near the end of files above a size limit

## Acceptance criteria

- [ ] Archiving never touches a file written in the last hour
- [ ] Hidden-agent count shown in the header

## Test plan

- Unit tests with temporary directories

## Notes

—
