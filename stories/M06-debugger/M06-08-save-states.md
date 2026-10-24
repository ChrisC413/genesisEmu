# M06-08 · Save states

**Milestone:** [M06 – Debugger & freeze execution](README.md)  
**Status:** Draft  
**Depends on:** M06-01  
**Spec refs:** [08-debugger](../../docs/specs/08-debugger.md#save-states)

## Story

As a **reverse engineer**, I want save and load full machine state to slots and named files, so that I can return to an interesting moment instantly.

## Scope

- Chunked, versioned format; ROM SHA-1 check
- Slots 1–9 + named states with thumbnails
- Load/save at instruction boundaries
- Hotkeys

## Acceptance criteria

- [ ] Determinism: save → run 600 frames → hash equals load → run 600 frames → hash
- [ ] State from another ROM refused
- [ ] Coverage unaffected by loading

## Test plan

- `[rom]` determinism test
- Unit tests for chunk versioning
