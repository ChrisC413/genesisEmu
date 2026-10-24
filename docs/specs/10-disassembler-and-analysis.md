# 10 · Disassembler and Analysis Toolset

## Pipeline

```
ROM bytes ─► seeds (vectors, header, coverage exec starts, user marks)
          ─► recursive-descent code discovery (68k)  ─┐
          ─► Z80 driver discovery (from provenance)  ─┼─► regions: code / data(typed) / unknown
          ─► jump-table & pointer-table detection    ─┘
          ─► functions + cross-references
          ─► symbols (auto, imported, user, LLM-proposed)
          ─► listing view  ─► reassemblable export (AS) ─► round-trip verification
```

All results are stored in the project database (spec 12) and are incremental: re-running discovery after
more coverage only adds/updates regions; it never overwrites user decisions.

## Code discovery

- **Seeds**: reset vector, all exception/interrupt vectors, `EXEC_68K_OPCODE` addresses from coverage (strongest evidence), user-marked entry points.
- **Recursive descent** following fallthrough, branches, calls, and resolvable jumps. Stops at returns, unconditional jumps, illegal opcodes, or bytes already claimed as data.
- **Confidence levels**: `executed` > `reached-statically` > `heuristic`. Conflicts (code path running into executed data) are flagged for review, not silently resolved.
- **Jump tables** — recognize the common Sonic idioms:
  - `move.w table(pc,d0.w),d1` / `jmp table(pc,d1.w)` (relative word offset tables — object routine dispatch),
  - `add.w d0,d0` / `move.w off(pc,d0.w),d0` / `jsr off(pc,d0.w)`,
  - long-pointer tables (`movea.l table(pc,d0.w),a1` / `jsr (a1)`).
  Table bounds come from coverage (observed indices) plus heuristics (stop at first entry not pointing to plausible code); unresolved bounds are flagged.
- **Indirect calls** observed at runtime (`jsr (a0)`) record their observed targets as xrefs.

## Data typing

User-applicable types (also auto-suggested): `byte/word/long`, `pointer (abs/rel to label)`, `ascii`,
`tile art (raw 4bpp)`, `palette`, `plane mapping`, `sprite mapping`, `DPLC`, `Nemesis / Kosinski / Enigma /
Kosinski-M / Saxman / Comper` compressed blocks, `SMPS music/SFX`, `level layout`, `object placement`,
`collision`, `padding`, and user-defined **structs** (e.g. the SST object struct). Typed data renders with a
preview (tiles/palettes) in the listing.

## Symbols

| Source | Example | Precedence |
|--------|---------|------------|
| User | `Sonic_Jump` | highest |
| LLM proposed + accepted | `Obj_RingCollect` | |
| Reference import (Sonic Retro) | `Sonic_Jump` | |
| Hardware | `VDP_control_port`, `Z80_bus_request` | |
| Auto | `sub_01A2C4`, `loc_01A2D0`, `byte_02F000`, `ram_FFB000` | lowest |

- Every symbol keeps its **source** and history (renames are versioned) — needed for blind scoring (spec 11).
- RAM symbols (68k `$FF0000–$FFFFFF`, Z80 RAM) and struct-field names (`obj.x_pos` at SST offset `$08`) are first-class.
- Local labels inside functions (`.loop`) supported in export.
- Comments: line comments, block comments, function headers.

## Reference label import (D-004)

- Input: the symbol/map output produced by building a Sonic Retro disassembly with AS (address → label), plus optional parsing of the source for comments and data-type hints.
- Imported labels go into the `reference` source, linked to the disassembly commit hash.
- Importer verifies alignment: the built ROM's SHA-1 must equal the loaded ROM's SHA-1; otherwise import is refused (or allowed with an explicit offset map).
- Modes: **visible** (labels shown, normal work) and **hidden** (stored but not shown or sent to the LLM — for blind evaluation).

## Listing view (UI)

Scrollable, virtualized listing of the whole ROM: address, bytes, label, instruction/data, comment, xref count.
Navigation: go to address/label, back/forward history, follow operand (Enter), xrefs popup (X), rename (N),
comment (;), set type (D/C/A keys as in common RE tools). Synchronized with the ROM map and debugger.

## Reassemblable export

- Target: **AS** (D-010) syntax; a generated `build` script for Windows and macOS.
- Emitted source must be **bit-exact**: always explicit size suffixes, force branch sizes (`bra.w`), disable AS optimizations that change encodings (e.g. `move.l #imm` → `moveq`), use absolute-word vs absolute-long exactly as encoded, emit `dc.w` for any instruction whose encoding the assembler can't reproduce.
- Every pointer that is recognized becomes a label reference (so code/data can move after mods); unrecognized pointers stay numeric and are listed in a **relocation-risk report**.
- Layout: one file per region group (e.g. `code/`, `art/`, `sound/`), binary data extracted to files and `binclude`d.
- **Round-trip verification**: export → assemble → byte-compare with the original ROM; first difference reported with address and symbol.
- Z80 driver code exported as a separate AS Z80 source assembled and included.

## Scoring against the reference

- Code/data boundary accuracy: % of bytes whose code/data classification matches the reference disassembly.
- Function boundary agreement, jump-table bound agreement.
- Reported per ROM in an analysis dashboard and tracked over time.
