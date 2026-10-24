# M02-04 · VDP port stub: status, HV counter, V-int

**Milestone:** [M02 – System bus & boot](README.md)  
**Status:** Draft  
**Depends on:** M02-01, M02-05  
**Spec refs:** [05-vdp](../../docs/specs/05-vdp.md#ports)

## Story

As a **developer**, I want a VDP stub that accepts commands, stores memories, reports status and raises V-int, so that boot code that polls the VDP doesn't hang.

## Scope

- Control/data port command latch, register writes, VRAM/CRAM/VSRAM writes (no rendering)
- Status register V-blank/H-blank bits from scanline position
- V-int at line 224 when enabled

## Acceptance criteria

- [ ] Sonic 1 V-int handler executes once per frame
- [ ] VRAM contents after boot are plausible (non-zero tiles)

## Test plan

- Unit tests for command parsing
- Boot test

## Notes

This grows into the full VDP in M03-01.
