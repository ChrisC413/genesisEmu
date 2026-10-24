# M02-01 · 68k memory map and bus

**Milestone:** [M02 – System bus & boot](README.md)  
**Status:** Done  
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

- [x] Unit tests for every range in the spec 04 table — `tests/unit/bus_test.cpp` (ROM, past-ROM / expansion / 32X open bus, Z80 area, I/O, BUSREQ / RESET, TMSS, VDP mirrors, RAM mirror); `$A130F1` is stored only (SRAM is M04-08)
- [x] RAM mirror `$E00000` aliases `$FF0000` — all 32 mirror pages share one fast page
- [x] Unmapped writes logged with PC — `SystemBus::issues()` records address, PC, value, size and kind (unmapped, ROM write, Z80 not owned, VDP locked); `gen_cli run` prints totals and the first entries

## Test plan

- Unit tests with a synthetic ROM

## Notes

- Implementation: `src/core/bus/system_bus.{hpp,cpp}`. ROM pages beyond the image (padded to 64 KB) are unmapped. Open bus is the CPU's prefetch word (IRC).
- `gen_core` has no logging dependency, so the "log" is an in-memory record list (first 4096 entries plus per-kind counters) rather than spdlog output.
- Fetch vs data reads are already distinguished by the CPU's `onAccess` calls (M01); a bus test checks it.
