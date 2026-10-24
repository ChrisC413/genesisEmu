# 05 · VDP (Video Display Processor)

## Memories

| Memory | Size | Format |
|--------|------|--------|
| VRAM | 64 KB | Tiles (8×8, 4bpp, 32 bytes), name tables, sprite table, H-scroll table |
| CRAM | 64 × 9-bit | 4 palettes × 16 colors, `0000 BBB0 GGG0 RRR0` |
| VSRAM | 40 × 10-bit | Vertical scroll values (plane A/B per 2-cell column) |
| Registers | 24 × 8-bit | Written via control port `$8000 | (reg << 8) | value` |

## Ports

- **Control port** (`$C00004`): register writes; two-word command writes setting address + code (VRAM/CRAM/VSRAM read/write, DMA bit). Reads return the **status register**: FIFO empty/full, V-int pending, sprite overflow, sprite collision, odd frame, in V-blank, in H-blank, DMA busy, PAL.
- **Data port** (`$C00000`): reads/writes at the current address, auto-increment by register 15.
- **HV counter** (`$C00008`): V counter (with V28/V30 and interlace jump tables) and H counter (approximated from MCLK position in the line).

Command-word pending-state rules (first word latched, second word completes; register write cancels pending) must match hardware since Sonic games rely on them.

## Display modes

- H32 (256 px) and **H40 (320 px)**; V28 (224 lines) and V30 (240 lines, PAL only).
- **Interlace mode 2** (double-resolution, 8×16 tiles): required for Sonic 2 2P mode. Output 320×448 frames, alternating fields.
- Interlace mode 1: render as progressive (fields identical).
- Display disable, background color (reg 7), left-8-pixel blanking (reg 0 bit 5).

## Rendering

Scanline renderer producing RGBA8888 into a 320×240 (or 320×480 interlaced) framebuffer.

For each line:
1. Fetch **plane B**, **plane A** (or **window** where the window region applies) tiles with scrolling:
   - H-scroll modes: full screen, per 8-line cell, per line (reg 11).
   - V-scroll modes: full screen, per 2-cell column (reg 11), with the leftmost-column quirk approximated.
   - Plane sizes 32/64/128 cells (reg 16), including invalid combinations as hardware mirrors.
2. **Sprites**: parse the sprite attribute table via link list (max 80 in H40 / 64 in H32), per-line limits (20 / 16 sprites, 320 / 256 pixels), sprite masking (x = 0 rule), overflow and collision flags.
3. **Priority**: per-pixel composition of B low, A low, sprite low, B high, A high, sprite high.
4. **Shadow/highlight** (reg 12 bit 3): palette-3 color 14/15 sprite operators, shadow by low-priority planes.
5. Palette lookup from CRAM **at the moment the line is rendered**, so mid-frame CRAM writes from H-int (water effects in S1/S2/S3K) appear on the correct lines.

Layer debug toggles: individually hide B, A, window, sprites; show only high/low priority.

## DMA

| Type | Behavior |
|------|----------|
| 68k → VRAM/CRAM/VSRAM | Source from ROM/RAM; 68k frozen for the transfer duration (approx. bandwidth: 18 bytes/line active display in H40, 205 bytes/line in V-blank; configurable table) |
| VRAM fill | Triggered by data-port write after the command; fills with the high byte |
| VRAM copy | Byte copy within VRAM |

DMA length/source registers (19–23) update as hardware does after the transfer. Every DMA produces an
`onDma` hook event with source address, destination type and address, and length — this feeds the ROM
map's "graphics/palette data" classification (spec 09).

## Interrupts and timing

- V-int at the first line of V-blank (224 in V28), when reg 1 bit 5 enabled; level 6 on the 68k; also asserts Z80 /INT.
- H-int counter (reg 10) reloaded at every line in V-blank, decremented each active line; on underflow raise level 4 if reg 0 bit 4 enabled.
- Interrupt acknowledge clears the pending flag (V-int pending is also visible in status bit 7).
- Frame timing per spec 01 (3420 MCLK per line).

## Accuracy limits (accepted)

No FIFO-precise timing, no mid-line register changes, no CRAM dots. A small FIFO model (4 entries) exists only
so the status register's FIFO bits behave plausibly.

## Inspection API

Expose VRAM/CRAM/VSRAM/registers, decoded sprite table, per-line scroll values, and per-line "which CRAM
writes happened on this line" for the VDP viewers (spec 08).
