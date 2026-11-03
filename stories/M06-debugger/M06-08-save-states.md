# M06-08 · Save states

**Milestone:** [M06 – Debugger & freeze execution](README.md)  
**Status:** Draft  
**Depends on:** M06-01  
**Spec refs:** [08-debugger](../../docs/specs/08-debugger.md#save-states)

## Story

As a **reverse engineer**, I want save and load full machine state to slots and named files, so that I can return to an interesting moment instantly.

## Already delivered by M06-00a

[M06-00a](M06-00a-play-from-snapshot.md) pulled the core forward: the chunked, versioned format with the ROM
SHA-1 check and state-hash verification, save/load of every component, slots 1-9 with hotkeys and File menus,
`genesisEmu --state=<file>`, `gen_cli --load-state` / `--save-state-at`, and both determinism tests. What is
left here: named states with 160×112 thumbnails (a file dialog and a state browser), the coverage criterion
once coverage exists (M07), and reading older chunk versions when a chunk first changes.

## Scope

- Chunked, versioned format; ROM SHA-1 check
- Slots 1–9 + named states with thumbnails
- Load/save at instruction boundaries
- Hotkeys

## Acceptance criteria

- [x] Determinism: save → run 600 frames → hash equals load → run 600 frames → hash (M06-00a)
- [x] State from another ROM refused (M06-00a)
- [ ] Coverage unaffected by loading

## Test plan

- `[rom]` determinism test
- Unit tests for chunk versioning
