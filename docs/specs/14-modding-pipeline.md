# 14 · Reassembly and the Modding Pipeline

All mods are made on the **reassemblable source** exported in M08 (D-005) and must run on **real hardware**
(D-017). No widescreen or other changes that break the hardware model. C static recompilation is in the
[backlog](../../stories/backlog/README.md).

## Pipeline

1. Export a bit-exact AS source tree from the project (spec 10).
2. Edit the source (by hand, with LLM help in an editor, or through generated modules such as splices — spec 15).
3. **Build ROM** in the app invokes AS (path configured; a fetch script downloads it on Windows and macOS),
   fixes the header checksum, and runs the result in a **derived project** that inherits the original's
   symbols by label name.
4. **Mod diff tools**:
   - Byte diff and symbol-level diff (functions changed/moved/added).
   - Coverage diff: run the same scripted input on original and mod; show code paths gained/lost.
   - Frame diff at checkpoints (golden hashes from the original run) to confirm "nothing else changed".
5. Relocation safety: the export's relocation-risk report (unresolved pointers) is shown before building; changes
   that move code past an unresolved pointer produce a warning.
6. **Hardware validation** (below) before a mod is considered done.

## Hardware-strict emulation mode

An emulator mode that surfaces behavior that works in emulators but fails on consoles:

| Check | Behavior in strict mode |
|-------|-------------------------|
| Uninitialized RAM | 68k RAM, Z80 RAM and VRAM start with pseudo-random contents (seeded, reproducible) |
| TMSS | Enabled (model 1 VA6+ behavior): VDP locked until `'SEGA'` written to `$A14000` |
| Z80 bus | Access to Z80 space without a granted bus request → warning + break option |
| VDP bandwidth | Warning when VRAM/CRAM writes during active display exceed the per-line slot budget |
| Odd-address word access | Address error (already modeled) + highlighted in the log |
| Cartridge registers | Writes to `$A130xx` with no device claiming them → warning |
| Header | Checksum, ROM end address and region fields validated after each build |

## Hardware validation

- Checklist per mod (stored in the derived project): strict mode run clean for the mod's scripted tests,
  header valid, ROM size fits the target cartridge (spec 16 for the LED cart), tested on a flash cart on at
  least one model 1 and one model 2 console (NTSC).
- Results recorded as a short report attached to the mod's PR.

## Special-stage mods for Sonic 2

| Mod | Description |
|-----|-------------|
| **Retry after failure** | When a special stage is failed, show a "Retry? Yes/No" prompt; *Yes* restarts the same stage with the same entry state, *No* returns to the level as normal (Q14 for costs/limits). Build-time options control limits. |
| **Special-stage modifications** | Data-driven changes to S2's halfpipe stages: ring requirements per checkpoint, layouts/object placement, timing, difficulty. Supported by special-stage data viewers in the analysis UI. |
| **Spliced stages** | Stages transplanted from Sonic 1 / S3K — see spec 15. |
| **Emerald LEDs** | Writes to the cartridge on each emerald collected — see spec 16. |

Each mod is a separate, toggleable patch set (build-time flags in the generated source), so they can be
combined and tested independently.
