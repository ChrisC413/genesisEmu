# M03-03 · H-int, V-int and HV counter

**Milestone:** [M03 – VDP video](README.md)  
**Status:** Done (game-level LZ check needs M04 input)  
**Depends on:** M03-01  
**Spec refs:** [05-vdp](../../docs/specs/05-vdp.md#interrupts-and-timing)

## Story

As a **developer**, I want accurate per-line interrupt timing and HV counter values, so that water effects and raster tricks happen on the correct lines.

## Scope

- H-int counter reload/decrement
- V-int pending flag, acknowledge
- HV counter tables for H32/H40, V28/V30, interlace
- Z80 /INT assertion for one line

## Acceptance criteria

- [ ] Labyrinth Zone water line in Sonic 1 appears at the right height — **not verified**: LZ is not part of the Sonic 1 attract demos, so it needs scripted input (M04). The same mechanism (H-int changing CRAM mid-frame) was checked on Sonic 2's ARZ attract demo: the underwater palette starts exactly at the water surface (golden checkpoint, frame 8000), and a unit test checks that a CRAM write during line N shows from line N on
- [x] Unit tests for counter sequences — H-int every N+1 lines including line 224, reload in V-blank, V-int and Z80 /INT for one line, acknowledge, V-int firing when enabled late, HV counter jumps for H32/H40, V28/V30 NTSC/PAL and both interlace modes

## Test plan

- Unit tests; visual check

## Notes

- Per line the scheduler calls `Vdp::beginLine` (V-blank flag, V-int at the first V-blank line, Z80 /INT for that line), runs the 68k, then `Vdp::endLine` (render the line, step the H-int counter).
- The H-int counter is decremented after each of lines 0-224 and reloaded on the other lines. An underflow raises H-int at the end of the line, so the handler runs during the next line and its CRAM writes show from that line on (each line is rendered at its end).
- Z80 /INT is exposed as `VdpState::z80Int`; nothing consumes it until the Z80 runs (M04).
- Visual check: Sonic 2 ARZ attract demo, water palette from the surface line down; Sonic 1 GHZ (no water).
