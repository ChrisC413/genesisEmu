# M12-05 · Sonic 2 ROM mod: write to the cartridge on emerald collected

**Milestone:** [M12 – Sonic 2 cartridge hardware mod: emerald LEDs](README.md)  
**Status:** Draft  
**Depends on:** M12-02, M10-01  
**Spec refs:** [16-cartridge-hardware-mod](../../docs/specs/16-cartridge-hardware-mod.md#game-side-behavior-rom-mod)

## Story

As a **reverse engineer**, I want the S2 results path to call `CartLed_SetEmerald` writing `1 << n` to `EMERALD_SET`, so that the real cartridge can light its LEDs.

## Scope

- Patch in the exported S2 source as a toggleable patch set
- Called from native results and from the splice exit glue (M11-08)
- Optional boot read-back per Q15

## Acceptance criteria

- [ ] Collecting emerald n writes exactly `1 << n` once
- [ ] On a cart/emulator without the device, the game behaves identically (mod diff + strict mode)

## Test plan

- `[rom]` scripted test with `cartdev` checkpoint
- Mod diff
