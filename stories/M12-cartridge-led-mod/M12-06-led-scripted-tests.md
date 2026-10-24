# M12-06 · Scripted-test support for cartridge devices

**Milestone:** [M12 – Sonic 2 cartridge hardware mod: emerald LEDs](README.md)  
**Status:** Draft  
**Depends on:** M12-05, M04-03  
**Spec refs:** [13-testing-strategy](../../docs/specs/13-testing-strategy.md#scripted-input-format-tests-only), [16-cartridge-hardware-mod](../../docs/specs/16-cartridge-hardware-mod.md#emulator-support)

## Story

As a **developer**, I want a `cartdev` checkpoint in the scripted-input format, so that LED behavior is regression-tested.

## Scope

- `checkpoint <frame> cartdev emerald_leds == <mask>`
- Works in strict mode

## Acceptance criteria

- [ ] A scripted run collecting one emerald asserts mask `$01`

## Test plan

- `[rom]` CTest
