# 09 · Coverage and the Color-Coded ROM Map

The coverage recorder observes every instruction start and memory access (spec 01 hooks) and builds a
per-byte picture of how the ROM — and RAM — is used. The ROM map visualizes it.

## Recorded data (per ROM byte)

Stored in parallel shadow arrays sized to the ROM:

| Field | Type | Meaning |
|-------|------|---------|
| `flags` | `u16` bitset | See flag table below |
| `execCount68k` | `u32` (saturating) | Times an instruction **started** at this address (68k) |
| `firstFrame` / `lastFrame` | `u32` | Frame numbers of first and most recent execution |
| `framesExecuted` | `u32` | Number of distinct frames in which this instruction executed |
| `execThisFrame` | `u16` | Executions in the current frame (reset lazily when `lastFrame` changes) |
| `maxPerFrame` | `u16` | Highest executions seen in any single frame |
| `windowFrames` / `windowExecs` | `u16` / `u32` | Frames-executed and total executions within the observation window (for per-frame rate) |
| `watchExecCount` | `u32` | Executions since the active watch session was armed |
| `category` | `u8` | Semantic data category (see *Data categories*), from roles, DMA, sound attribution or user typing |
| `readerPc` | `u32` | PC of the most recent *data* reader (for reader attribution) |

Flags:

| Bit | Flag | Set when |
|-----|------|----------|
| 0 | `EXEC_68K_OPCODE` | An instruction started here |
| 1 | `EXEC_68K_OPERAND` | Byte is part of an executed instruction's extension words |
| 2 | `READ_68K` | 68k data read (not a fetch) |
| 3 | `READ_Z80` | Z80 read through the bank window (typically DAC samples) |
| 4 | `DMA_VRAM` | Byte was the source of a DMA to VRAM (tiles / maps) |
| 5 | `DMA_CRAM` | DMA source to CRAM (palettes) |
| 6 | `DMA_VSRAM` | DMA source to VSRAM |
| 7 | `READ_BY_ROLE` | Data read by a PC inside a routine with a tagged role (decompressor, sprite builder, …) — see *Routine roles* |
| 8 | `COPIED_TO_Z80_RAM` | Byte was copied into Z80 RAM (Z80 code/data provenance) |
| 9 | `EXEC_Z80_VIA_PROVENANCE` | The Z80 executed a RAM byte that originated here |
| 10 | `STATIC_CODE` | Static analysis (spec 10) believes this is code, never executed |
| 11 | `WATCH_NEW` | First executed (or first read) while a watch session was active |

For **68k RAM** and **Z80 RAM** the same structure exists (with `WRITE_*` flags and write counts), giving
RAM maps for variable discovery. Z80 code runs from Z80 RAM, so Z80 exec counts live on the Z80 RAM map and
are projected back to ROM through provenance.

### Provenance (Z80 code → ROM)
When the 68k writes into Z80 RAM (`$A00000–$A01FFF`), record for each Z80 RAM byte the ROM address of the
most recent 68k ROM read *by the same instruction or loop* (heuristic: last ROM data read by the 68k with
a value matching the written byte). Sonic 1 copies the DAC driver directly (high-confidence provenance);
Sonic 2/S3K decompress the driver (provenance is marked *low confidence* and attributed to the compressed
block as a whole). Provenance confidence is shown in the UI.

### Routine roles (reader attribution)
The user (or static analysis, or the LLM via accepted proposals) tags routines with a **role**. Data read by
a PC inside a tagged routine gets `READ_BY_ROLE` and the role's data category. This classifies data that is
never DMA'd directly — compressed art, mappings, level layouts.

| Role | Data it reads becomes |
|------|-----------------------|
| Decompressor (Nemesis / Kosinski / Enigma / Kosinski-M / Saxman / Comper) | Compressed (+ format) |
| Sprite builder (e.g. `BuildSprites`) / mapping loader / DPLC loader | Sprite mappings / DPLC |
| Palette loader / palette cycler | Palette |
| Level/layout loader, collision loader, object-placement loader | Level data |
| Sound driver (68k SMPS in Sonic 1; Z80 driver reads via bank window in S2/S3K) / sound queue | Sound (music / SFX) |
| Text / HUD renderer | Text / HUD |

### Data categories

`code-68k`, `code-z80`, `graphics` (tile art), `sprite` (mappings / DPLC / sprite art), `palette`,
`level`, `sound` (music, SFX, DAC samples, Z80 driver), `compressed:<format>` (shown with the category of
what it decompresses into, when known), `pointer-table`, `text`, `other-data` (read but unattributed),
`padding`, and **`not-used`** (never touched by any CPU or DMA in any included layer, and not reached by
static analysis).

Automatic sources, in priority order: user typing (spec 10) > routine role > DMA destination (VRAM → graphics,
CRAM → palette, VSRAM → level/scroll) > Z80 bank-window read (→ sound) > copied to Z80 RAM (→ sound driver) >
plain read (→ other-data).

## Execution tiers (D-021)

Each executed instruction is in exactly one tier, per CPU:

| Tier | Rule (defaults configurable) | Typical Sonic code |
|------|------------------------------|--------------------|
| **Once** | `execCount == 1` | Boot, hardware init, one-time setup |
| **More than once** | `execCount > 1`, not per-frame | Level load, event handlers, menus |
| **Per frame** | Executed in ≥ 95% of frames in the observation window **and** `windowExecs / windowFrames ≤ 1.5` | V-int handler, main game loop, HUD update |
| **More than per frame** | Executed in ≥ 95% of frames **and** more than 1.5 executions per frame on average | Object routines (run per object), inner loops, DMA queue processing |

The *observation window* defaults to the last 300 frames, or the active watch session. Code that ran every
frame earlier but not in the current window (e.g. title-screen loop during gameplay) falls back to
*more than once*. Each tier is also shown with its "max per frame" value in tooltips.

Watch-session overlays (independent of tier):

| Overlay | Rule |
|---------|------|
| **Watch: new** | First executed after the watch session was armed |
| **Watch: once-after-event** | `WATCH_NEW` and `watchExecCount == 1` (true one-shot reactions to the event) |

## Watch sessions

A watch session answers: *"what code runs because of this event?"*

1. User arms a watch (button or hotkey), optionally naming it ("ring collected", "boss hit").
2. Recorder snapshots the current executed set as the baseline.
3. While active: newly executed addresses get `WATCH_NEW`; `watchExecCount` accumulates; per-frame periodicity is computed over the session only.
4. User stops the watch (or it auto-stops after N frames). The result is saved as a **layer**: the set of new addresses, their counts, and the frame range.
5. Layers can be compared: "code in layer A but not B" (e.g. diff "jump" vs "no jump" sessions).

Watches can also be armed automatically by a breakpoint action ("arm watch when `$FFFE10` changes").

## ROM map view

- **Linear layout**: each row is a fixed number of bytes (default 1024 per row at 1 px = 4 bytes); zoom from 1 px = 1 byte to 1 px = 1 KB. Optional Hilbert-curve layout for locality.
- Rendered as a texture updated incrementally (dirty rows) at most every 250 ms while running.
- **Color scheme** (default; configurable; colorblind-safe alternative palette provided):

| Category | Color |
|----------|-------|
| 68k code — more than per frame | `#C1121F` dark red |
| 68k code — per frame | `#FF3B30` red |
| 68k code — more than once | `#FF9500` orange |
| 68k code — once | `#FFD60A` yellow |
| 68k code — watch: new | `#AF52DE` purple (overrides the tiers when a watch layer is selected) |
| Z80 code (via provenance) | `#34C759` green (shade by the same four tiers) |
| Graphics (tile art) | `#5E5CE6` indigo |
| Sprite (mappings / DPLC) | `#BF5AF2` violet |
| Palette | `#FF2D55` pink |
| Level data | `#30B0C7` teal |
| Sound (music / SFX / samples / driver) | `#64D2FF` cyan |
| Compressed (unknown contents) | `#8E5A3C` brown (compressed blocks with known contents use that category with a hatched pattern) |
| Pointer table / text | `#A2845E` tan |
| Other data (read, unattributed) | `#0A84FF` blue |
| Static code, never executed | `#8E8E93` gray striped |
| Not used | `#1C1C1E` near-black |
| Padding (runs of `$FF` / `$00` ≥ 16 bytes) | `#2C2C2E` dark gray dotted |

When a byte has multiple categories, priority is: code > typed/role category > DMA category > other data >
static code > not used; a small corner tick indicates mixed use (e.g. jump tables both read as data and
adjacent to code).

- **Overlays**: category (default), execution tier, heat map (log of exec/read counts), selected watch layer, layer diff, **subsystem footprint** (spec 15), and "touched this frame" flash mode (live).
- **Filters**: per-CPU, per-category toggles; address range.
- **Legend** with live byte counts and % of ROM per category.

## Interaction

- Hover: tooltip with address, symbol (if any), categories, counts, first/last frame, reader PC, provenance.
- Click: open the address in the disassembly view (code) or the hex view (data).
- Drag-select: range stats; actions — label, mark as data type, add to analysis queue, create breakpoint.
- Bookmarks / regions with user notes, rendered as outlines on the map.

## Persistence

Coverage accumulates per project (spec 12) across sessions; each session is also stored as a separate layer
so it can be included/excluded. Export: CSV of ranges with categories, and a PNG of the map.

## Performance

Recording cost: one array OR + increment per access on the hot path; per-frame bookkeeping is O(executed
instructions this frame) using a dirty list. Must meet the budget in spec 01.
