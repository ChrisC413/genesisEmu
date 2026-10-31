# M02-04 · VDP port stub: status, HV counter, V-int

**Milestone:** [M02 – System bus & boot](README.md)  
**Status:** Done  
**Depends on:** M02-01, M02-05  
**Spec refs:** [05-vdp](../../docs/specs/05-vdp.md#ports)

## Story

As a **developer**, I want a VDP stub that accepts commands, stores memories, reports status and raises V-int, so that boot code that polls the VDP doesn't hang.

## Scope

- Control/data port command latch, register writes, VRAM/CRAM/VSRAM writes (no rendering)
- Status register V-blank/H-blank bits from scanline position
- V-int at line 224 when enabled

## Acceptance criteria

- [x] Sonic 1 V-int handler executes once per frame — `[rom]` boot test: exactly one level-6 interrupt in each of the last 60 of 600 frames (Sonic 2: 58 of 60, one lag frame)
- [x] VRAM contents after boot are plausible (non-zero tiles) — more than 4096 non-zero VRAM bytes after 600 frames of Sonic 1

## Test plan

- Unit tests for command parsing
- Boot test

## Notes

This grows into the full VDP in M03-01.

- `src/core/vdp/vdp.{hpp,cpp}`. Unit tests: `tests/unit/vdp_test.cpp`.
- Status: FIFO always empty, upper bits fixed at `$34xx`, H-blank from an approximate H counter (210 counts per line in H40, 171 in H32, with the hardware jumps). V counter follows the NTSC/PAL V28/V30 jump tables and both interlace modes.
- DMA is implemented instantly (68k → VRAM/CRAM/VSRAM, fill, copy) without freezing the 68k and raises `onDma`; M03-02 adds bandwidth timing and edge cases. `DmaEvent` is now defined in `vdp.hpp`.
- H-int counter: decremented on lines 0-224, reloaded on the other lines.
