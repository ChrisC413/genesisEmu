# M12-04 · Prototype mode: LEDs driven by RAM watch

**Milestone:** [M12 – Sonic 2 cartridge hardware mod: emerald LEDs](README.md)  
**Status:** Draft  
**Depends on:** M12-03  
**Spec refs:** [16-cartridge-hardware-mod](../../docs/specs/16-cartridge-hardware-mod.md#emulator-support)

## Story

As a **reverse engineer**, I want virtual LEDs driven by a RAM watch on unmodified Sonic 2's emerald state, so that LED behavior and trigger events can be validated before the ROM mod exists.

## Scope

- Configurable RAM address/mask → LED mapping
- Uses watchpoint infrastructure

## Acceptance criteria

- [ ] Collecting an emerald in unmodified S2 lights the matching LED

## Test plan

- `[rom]` scripted test
