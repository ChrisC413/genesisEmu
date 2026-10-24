# M10-01 · Build modded ROM from the app

**Milestone:** [M10 – Modding pipeline & first Sonic 2 mods](README.md)  
**Status:** Draft  
**Depends on:** —  
**Spec refs:** [14-modding-pipeline](../../docs/specs/14-modding-pipeline.md#pipeline)

## Story

As a **reverse engineer**, I want a Build ROM command that assembles an edited export, fixes the header and runs it, so that iterating on mods is fast.

## Scope

- AS path config + fetch script (Windows and macOS)
- Header checksum and ROM-end fix-up
- Build output/errors panel with clickable source locations
- Derived project inheriting symbols by label name
- Mods as toggleable patch sets (build-time flags)

## Acceptance criteria

- [ ] Edit → build → run in < 10 s for Sonic 2
- [ ] Assembler errors navigate to the source line
- [ ] Unmodified export builds a byte-identical ROM

## Test plan

- `[rom]` test building an unmodified export
