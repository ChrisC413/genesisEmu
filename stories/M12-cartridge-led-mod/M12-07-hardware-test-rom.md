# M12-07 · Hardware bring-up test ROM

**Milestone:** [M12 – Sonic 2 cartridge hardware mod: emerald LEDs](README.md)  
**Status:** Draft  
**Depends on:** M12-02  
**Spec refs:** [16-cartridge-hardware-mod](../../docs/specs/16-cartridge-hardware-mod.md#emulator-support)

## Story

As a **developer**, I want a small homebrew ROM that walks the LEDs, exercises set/clear, and shows ID/read-back results on screen, so that the physical cartridge can be brought up independently of Sonic 2.

## Scope

- Built with AS from this repo (no commercial code)
- On-screen results; controller to step through tests
- Runs in the emulator with the device enabled

## Acceptance criteria

- [ ] All tests pass in the emulator
- [ ] Test ROM is redistributable (MIT)

## Test plan

- Emulator run in CI (homebrew ROM)
