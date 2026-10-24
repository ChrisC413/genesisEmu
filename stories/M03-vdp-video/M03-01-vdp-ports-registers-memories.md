# M03-01 · VDP ports, registers and memories

**Milestone:** [M03 – VDP video](README.md)  
**Status:** Done  
**Depends on:** —  
**Spec refs:** [05-vdp](../../docs/specs/05-vdp.md#ports), [05-vdp](../../docs/specs/05-vdp.md#memories)

## Story

As a **developer**, I want the full VDP command interface with hardware-accurate latch behavior, so that data lands in VRAM/CRAM/VSRAM exactly as on hardware.

## Scope

- All 24 registers decoded
- Command word latch rules; read/write codes; auto-increment
- Data-port reads with read-ahead buffer
- Byte-write duplication
- Status register incl. FIFO approximation

## Acceptance criteria

- [x] Unit tests for every command code and latch edge case — `tests/unit/vdp_test.cpp`: VRAM/CRAM/VSRAM read and write codes, the undocumented 8-bit VRAM read (`$0C`), invalid codes dropped, pending cleared by data/status access, the A15-A14 latch reused by a lone first word, register writes reloading CD1-CD0 / A13-A0, DMA bit ignored while DMA is disabled
- [x] CRAM/VSRAM wraparound correct — CRAM wraps at 64 words; VSRAM writes to entries 40-63 are dropped and the address wraps at 64 words

## Test plan

- Unit tests; free VDP test ROMs where available

## Notes

- `src/core/vdp/vdp.{hpp,cpp}` (ports, DMA, interrupts) and `vdp_render.cpp` (M03-04..09).
- A register write also loads A13-A0 and CD1-CD0 (= 2, no valid target), as on hardware; a lone first command word keeps A15-A14 from the last second word.
- Data-port reads keep a read buffer: CRAM/VSRAM reads fill the unused bits from it.
- FIFO model: 4 entries, filled by data-port writes during active display and drained at the end of each line (status bits only; writes never stall the 68k).
- The VDP keeps a sprite attribute cache (Y, size, link of each entry) updated only by VRAM writes inside the sprite table, like hardware; moving the table does not reload it.
- Byte writes to the ports are duplicated onto both halves by the bus (M02-01, B-5).
- Not modeled: Mode 4 (SMS) registers, the HV latch (reg 0 bit 1), 128 KB VRAM mode. No free VDP test ROM was run (`tools/fetch-test-data` fetches none).
