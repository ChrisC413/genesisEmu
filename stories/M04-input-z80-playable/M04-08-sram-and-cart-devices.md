# M04-08 · Cartridge device chain, SRAM and `$A130F1`

**Milestone:** [M04 – Input, Z80, playable](README.md)  
**Status:** Done  
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

- [x] S3K save slot survives app restart
- [x] Disabling SRAM restores ROM visibility at `$200000`
- [x] A test device claiming `$A130E1` receives writes; unclaimed `$A130xx` writes are logged

## Test plan

- Unit tests; manual S3K save test

## Notes

- `cart::ICartDevice` (src/core/cart/cart_device.hpp): claims `/TIME` registers, overlays cartridge space
  (pages it touches go through the chain before the ROM) and snoops ROM-space writes; part of the state hash.
- `cart::SramDevice`: header SRAM declaration (for a lock-on image, the locked-on cartridge's header at
  `$200100`), byte lanes, `$A130F1` map / write-protect bits, mapped from power on unless the ROM covers the
  SRAM range (S3K). S3K's writes at `$2000xx` now land in SRAM instead of the ROM-write log.
- Persistence: `<user data>/roms/<sha1>/<sha1>.srm` (`common::sramPath/loadSram/saveSram`). The app loads it
  when a ROM starts and writes it every 2 s while dirty, on ROM change and on exit; `gen_cli run --sram`.
- Verified: a scripted S3K run that picks data-select slot 1 and starts AIZ writes the .srm; a second run
  that loads it shows slot 1 as "ZONE 1" with lives instead of "NEW" (frame checked); the app logs
  "battery save loaded" for the same file. Restart persistence was exercised through gen_cli and the app's
  load path; saving from inside an interactive app session is the same code and still deserves a manual
  check.
