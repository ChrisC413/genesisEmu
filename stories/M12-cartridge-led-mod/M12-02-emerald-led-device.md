# M12-02 · Emulated emerald-LED cartridge device

**Milestone:** [M12 – Sonic 2 cartridge hardware mod: emerald LEDs](README.md)  
**Status:** Draft  
**Depends on:** M12-01, M04-08  
**Spec refs:** [16-cartridge-hardware-mod](../../docs/specs/16-cartridge-hardware-mod.md#emulator-support), [04-memory-bus](../../docs/specs/04-memory-bus.md#cartridge-device-chain)

## Story

As a **developer**, I want an `EmeraldLedDevice` implementing the register interface with persistence and save-state support, so that the mod can be developed and tested entirely in the emulator.

## Scope

- `ICartDevice` implementation per spec 16
- Persist to `<sha1>.cartdev.json`
- Enable per SHA-1 / manual
- Save-state chunk

## Acceptance criteria

- [ ] Set-only semantics; clear sequence; ID/read-back behavior per spec
- [ ] State survives restart and save/load

## Test plan

- Unit tests
