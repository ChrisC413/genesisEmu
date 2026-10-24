# M08-01 · Project database

**Milestone:** [M08 – Disassembly & analysis](README.md)  
**Status:** Draft  
**Depends on:** —  
**Spec refs:** [12-project-database](../../docs/specs/12-project-database.md)

## Story

As a **developer**, I want the SQLite project database with migrations and text export, so that all analysis is stored durably and diffably.

## Scope

- Schema per spec 12
- Migrations + backup on upgrade
- Transactional writes, undo stack for renames/types
- Text export/import (`genproj`)

## Acceptance criteria

- [ ] Open/close/reopen preserves all data
- [ ] Text export is deterministic (same DB → same files)
- [ ] Import(export(db)) == db for exported tables

## Test plan

- Unit tests
