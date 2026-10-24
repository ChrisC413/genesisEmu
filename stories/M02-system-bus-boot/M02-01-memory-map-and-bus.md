# M02-01 · 68k memory map and bus

**Milestone:** [M02 – System bus & boot](README.md)  
**Status:** Draft  
**Depends on:** —  
**Spec refs:** [04-memory-bus](../../docs/specs/04-memory-bus.md#68000-memory-map), [04-memory-bus](../../docs/specs/04-memory-bus.md#bus-requirements)

## Story

As a **developer**, I want the 68k bus decoding ROM, RAM, I/O, Z80 window and VDP ranges with mirrors and open bus, so that games read and write the right places.

## Scope

- Page table (64 KB pages) with fast paths for ROM/RAM
- Mirrors for RAM and VDP ports
- Open-bus reads, unmapped-access log
- Fetch vs data-read distinction in `onAccess`

## Acceptance criteria

- [ ] Unit tests for every range in the spec 04 table
- [ ] RAM mirror `$E00000` aliases `$FF0000`
- [ ] Unmapped writes logged with PC

## Test plan

- Unit tests with a synthetic ROM
