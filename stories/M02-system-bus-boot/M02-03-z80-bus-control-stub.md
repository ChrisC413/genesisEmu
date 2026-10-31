# M02-03 · Z80 bus request / reset and Z80 RAM access

**Milestone:** [M02 – System bus & boot](README.md)  
**Status:** Done  
**Depends on:** M02-01  
**Spec refs:** [04-memory-bus](../../docs/specs/04-memory-bus.md#z80-memory-map)

## Story

As a **developer**, I want BUSREQ/RESET registers and Z80 RAM accessible from the 68k (Z80 not yet executing), so that games can load their sound drivers during boot.

## Scope

- `$A11100` / `$A11200` semantics
- 68k access to `$A00000–$A0FFFF` when bus owned
- Z80 RAM kept in core state (8 KB)

## Acceptance criteria

- [x] Bus grant reads back correctly — bit 8 of `$A11100` (bit 0 of the byte) reads 0 while the bus is requested
- [x] Sonic 1 copies its DAC driver into Z80 RAM (bytes visible in state) — `[rom]` test: more than 256 non-zero bytes in Z80 RAM after 600 frames
- [x] Access without owning the bus is logged — `BusIssue::Z80NotOwned`, reads return open bus

## Test plan

- Unit tests

## Notes

- `src/core/z80/z80_control.{hpp,cpp}`: 8 KB RAM, BUSREQ / RESET, the serial bank register and YM2612 / PSG write stubs (YM status reads as ready). Word reads return the byte in both halves; word writes store the high byte.
- Simplification: the grant is immediate and does not depend on the reset line (some references only grant while reset is released). None of the three games needed more.
- No game waited on a Z80 handshake within 600 frames, so no workaround was needed.
