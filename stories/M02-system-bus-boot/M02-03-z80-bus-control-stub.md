# M02-03 · Z80 bus request / reset and Z80 RAM access

**Milestone:** [M02 – System bus & boot](README.md)  
**Status:** Draft  
**Depends on:** M02-01  
**Spec refs:** [04-memory-bus](../../docs/specs/04-memory-bus.md#z80-memory-map)

## Story

As a **developer**, I want BUSREQ/RESET registers and Z80 RAM accessible from the 68k (Z80 not yet executing), so that games can load their sound drivers during boot.

## Scope

- `$A11100` / `$A11200` semantics
- 68k access to `$A00000–$A0FFFF` when bus owned
- Z80 RAM kept in core state (8 KB)

## Acceptance criteria

- [ ] Bus grant reads back correctly
- [ ] Sonic 1 copies its DAC driver into Z80 RAM (bytes visible in state)
- [ ] Access without owning the bus is logged

## Test plan

- Unit tests
