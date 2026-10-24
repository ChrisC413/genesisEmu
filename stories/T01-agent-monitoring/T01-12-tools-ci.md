# T01-12 · Run agent-monitor tests in CI

**Milestone:** [T01 – Agent monitoring](README.md)  
**Status:** Draft  
**Depends on:** T01-02, M00-02  
**Spec refs:** [17-agent-monitoring](../../docs/specs/17-agent-monitoring.md#testing) (Testing)

## Story

As a **developer**, I want the tool's tests to run in GitHub Actions, so that protocol changes can't silently break the monitor.

## Scope

- Add a `python -m unittest discover -s tools/agent-monitor/tests` step to the Windows workflow created by M00-02

## Acceptance criteria

- [ ] CI fails when an agent-monitor test fails
- [ ] Adds under 30 s to the workflow

## Test plan

- CI run on a PR

## Notes

Waits for M00-02 (owned by the M00 agent) to avoid editing the workflow concurrently. T01-01..03 are marked Done with tests passing locally only.
