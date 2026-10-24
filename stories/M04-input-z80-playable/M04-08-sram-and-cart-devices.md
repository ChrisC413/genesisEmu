# M04-08 · Cartridge device chain, SRAM and `$A130F1`

**Milestone:** [M04 – Input, Z80, playable](README.md)  
**Status:** Draft  
**Depends on:** —  
**Spec refs:** [04-memory-bus](../../docs/specs/04-memory-bus.md#cartridge-device-chain), [04-memory-bus](../../docs/specs/04-memory-bus.md#cartridge-loading)

## Story

As a **player**, I want battery saves for Sonic 3 & Knuckles, built on a pluggable cartridge-device chain, so that my save files persist and custom cartridge hardware (the emerald-LED mod) can be added later.

## Scope

- `ICartDevice` chain claiming `/TIME` (`$A130xx`) addresses and optional ROM-space write snooping; devices in save states
- SRAM controller as the first device
- Odd-byte SRAM at `$200001+` per header
- `$A130F1` enable/write-protect switching ROM/SRAM
- Persist `<sha1>.srm` on exit and periodically when dirty

## Acceptance criteria

- [ ] S3K save slot survives app restart
- [ ] Disabling SRAM restores ROM visibility at `$200000`
- [ ] A test device claiming `$A130E1` receives writes; unclaimed `$A130xx` writes are logged

## Test plan

- Unit tests; manual S3K save test
