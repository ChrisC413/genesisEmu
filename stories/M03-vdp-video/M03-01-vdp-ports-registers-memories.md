# M03-01 · VDP ports, registers and memories

**Milestone:** [M03 – VDP video](README.md)  
**Status:** Draft  
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

- [ ] Unit tests for every command code and latch edge case
- [ ] CRAM/VSRAM wraparound correct

## Test plan

- Unit tests; free VDP test ROMs where available
