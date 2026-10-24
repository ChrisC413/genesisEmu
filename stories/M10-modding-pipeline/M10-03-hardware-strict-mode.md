# M10-03 · Hardware-strict emulation mode

**Milestone:** [M10 – Modding pipeline & first Sonic 2 mods](README.md)  
**Status:** Draft  
**Depends on:** M10-01  
**Spec refs:** [14-modding-pipeline](../../docs/specs/14-modding-pipeline.md#hardware-strict-emulation-mode)

## Story

As a **reverse engineer**, I want an emulation mode that flags behavior that works in emulators but fails on real consoles, so that mods work on real hardware the first time.

## Scope

- Checks from spec 14: random RAM/VRAM init (seeded), TMSS on, Z80 bus misuse, VDP bandwidth warnings, unclaimed `$A130xx` writes, header validation
- Warnings panel with PC and break option
- Strict mode selectable for scripted tests

## Acceptance criteria

- [ ] Unmodified Sonic 2 runs clean in strict mode (baseline)
- [ ] A test ROM violating each rule triggers the matching warning

## Test plan

- Unit tests with synthetic ROMs
