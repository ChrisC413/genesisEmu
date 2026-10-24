# M04-07 · Z80 system integration

**Milestone:** [M04 – Input, Z80, playable](README.md)  
**Status:** Draft  
**Depends on:** M04-05  
**Spec refs:** [03-cpu-z80](../../docs/specs/03-cpu-z80.md#requirements), [04-memory-bus](../../docs/specs/04-memory-bus.md#z80-memory-map)

## Story

As a **developer**, I want the Z80 running in the scheduler with bus arbitration, bank window, interrupts and sound-chip ports, so that games' Z80 drivers run and communicate with the 68k.

## Scope

- Catch-up execution per scanline
- BUSREQ/RESET halting
- Bank register serial writes; 68k window with stall penalties and translated-address reporting (CZ-6)
- V-blank /INT for one line
- YM2612/PSG port writes forwarded to stubs (register log)

## Acceptance criteria

- [ ] Sonic 2/S3K Z80 drivers run without hanging the 68k (handshakes complete)
- [ ] Register-write log shows plausible YM writes during title music

## Test plan

- `[rom]` boot tests extended
- Unit tests for bank register
