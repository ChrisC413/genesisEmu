# M11-01 · Workspace with donor and target projects

**Milestone:** [M11 – Special-stage splicing](README.md)  
**Status:** Draft  
**Depends on:** —  
**Spec refs:** [12-project-database](../../docs/specs/12-project-database.md#workspace-multiple-projects)

## Story

As a **reverse engineer**, I want a workspace holding several projects (donor + target) open at once, so that I can analyze and splice across games.

## Scope

- `.genws` workspace file; open multiple projects
- Cross-project references by `(sha1, space, address)`
- UI: project switcher; side-by-side listings

## Acceptance criteria

- [ ] Sonic 1 and Sonic 2 projects open together; cross-project link navigates correctly

## Test plan

- Unit tests for workspace I/O
