# M07-10 · Coverage persistence, layers and merge

**Milestone:** [M07 – ROM & memory exploration](README.md)  
**Status:** Draft  
**Depends on:** M07-04, M08-01  
**Spec refs:** [09-coverage-and-rom-map](../../docs/specs/09-coverage-and-rom-map.md#persistence), [12-project-database](../../docs/specs/12-project-database.md)

## Story

As a **reverse engineer**, I want coverage saved in the project per session, mergeable, and exportable, so that knowledge accumulates across many play sessions.

## Scope

- Each session stored as a layer; cumulative merged layer
- Include/exclude layers
- Export CSV of ranges + PNG of map
- `gen_cli coverage --input script` to build coverage headless

## Acceptance criteria

- [ ] Coverage reloads identically after restart
- [ ] Merging two sessions equals union of flags and sum of counts

## Test plan

- Unit tests
