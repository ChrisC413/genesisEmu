# T01-06 · Story-aware view (title and acceptance progress)

**Milestone:** [T01 – Agent monitoring](README.md)  
**Status:** Draft  
**Depends on:** T01-03  
**Spec refs:** [17-agent-monitoring](../../docs/specs/17-agent-monitoring.md#open-questions) (MQ9)

## Story

As a **developer**, I want to see each agent's story title and how many acceptance criteria are checked, so that progress is measured against the spec rather than self-reported.

## Scope

- Find the story file by ID in `stories/*/` on the agent's branch (`git show <branch>:<path>`)
- Show title, `Status:` value, and `[x]`/total acceptance criteria

## Acceptance criteria

- [ ] Works for stories edited on the branch but not yet on `main`
- [ ] Unknown story IDs degrade to the bare ID

## Test plan

- Unit tests on sample story markdown

## Notes

—
