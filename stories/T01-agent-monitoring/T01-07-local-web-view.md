# T01-07 · Optional local web view

**Milestone:** [T01 – Agent monitoring](README.md)  
**Status:** Draft  
**Depends on:** T01-02  
**Spec refs:** [17-agent-monitoring](../../docs/specs/17-agent-monitoring.md#open-questions) (MQ1)

## Story

As a **developer**, I want an optional browser view of the same data, so that I can keep it on a second screen or phone.

## Scope

- `--serve [host:port]` using `http.server`; default bind `127.0.0.1`
- `/api/snapshot` (the `--json` schema) and one static page polling it

## Acceptance criteria

- [ ] Read-only; no write endpoints
- [ ] Binding beyond localhost requires an explicit host argument

## Test plan

- Unit test of the snapshot endpoint

## Notes

Only if the user wants a browser view (MQ1).
