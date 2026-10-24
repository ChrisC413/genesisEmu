# M07-09 · 68k RAM and Z80 RAM maps

**Milestone:** [M07 – ROM & memory exploration](README.md)  
**Status:** Draft  
**Depends on:** M07-07  
**Spec refs:** [09-coverage-and-rom-map](../../docs/specs/09-coverage-and-rom-map.md#recorded-data-per-rom-byte)

## Story

As a **reverse engineer**, I want read/write heat maps of 68k RAM and Z80 RAM, so that I can discover variables, object slots and buffers.

## Scope

- Same view component in RAM mode
- Read vs write coloring, writer PC in tooltip
- Overlay of known struct layouts (e.g. SST slots) once defined

## Acceptance criteria

- [ ] Sonic 1 object RAM slots visible as repeating structure
- [ ] Writer PC navigates to code

## Test plan

- Manual UI test
