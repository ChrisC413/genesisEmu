# 00 · Overview

## Vision

genesisEmu is a Sega Genesis emulator whose primary purpose is **reverse engineering**. It runs Sonic 1,
Sonic 2 and Sonic 3 & Knuckles accurately enough to play, while recording *everything the hardware does*
so a human — assisted by an LLM — can turn the ROM into documented, reassemblable source code and then
modify it.

The concrete modding goals (D-017) are:

1. **Modify the special stages** of Sonic 2.
2. **Splice in special stages from other Sonic games** (Sonic 1, Sonic 3 & Knuckles) into Sonic 2.
3. Add **retry options after failure**.
4. Support a **Sonic 2 cartridge hardware mod**: the game writes to the cartridge each time an emerald is
   collected, and the cartridge lights one LED per emerald.

Every mod has to run on real hardware, not only in this emulator.

## Goals

| ID | Goal |
|----|------|
| G1 | Cross-platform desktop app for **Windows 11 (x64)** and **macOS 13+ (Apple Silicon; x64 best effort)** (D-015). |
| G2 | Full **video** (VDP) and **sound** (YM2612 + SN76489) output at full speed. |
| G3 | **Freeze execution** at any instruction boundary: pause, step, breakpoints, save states, and start tracing from a paused state. |
| G4 | **Hand-written, instruction-by-instruction** implementations of the **Motorola 68000** and **Zilog Z80** CPUs. |
| G5 | **ROM and memory exploration**: a color-coded map of the ROM showing, per CPU, code executed **once / more than once / per frame / more than once per frame**, code first run after a user-armed *watch event*, and data by what it is (graphics, sprite, palette, sound, level, compressed, not used) (D-021). |
| G6 | **Analysis toolset**: disassemble binary to assembly; discover code vs. data; manage symbols; export reassemblable source that rebuilds the ROM bit-for-bit. |
| G7 | **LLM-assisted decompilation**: convert assembly functions to higher-level pseudo-code and **interface contracts**, proposing symbol/label names that the user reviews and accepts — aimed at making special stages transplantable (D-019). |
| G8 | **Modding pipeline**: rebuild modified ROMs from the exported source, compare them with the original, and validate them for real hardware. |
| G9 | **Subsystem splicing**: extract a subsystem (a special stage) from a donor game's project together with everything it depends on, and transplant it into the Sonic 2 source (spec 15). |
| G10 | **Cartridge hardware mod support**: emulate the custom Sonic 2 emerald-LED cartridge register, and produce the interface spec the hardware is built against (spec 16). |

## Non-goals (for now)

- Sega CD, 32X, Master System / Mark III compatibility mode, Pico.
- Bus-cycle / sub-instruction accuracy (see D-002). Games that depend on it may glitch.
- Rewind, input movies, netplay, shaders, achievements (see D-007; rewind/movies may be revisited, Q9).
- **Widescreen** or other changes that break the original hardware model (D-017).
- C static recompilation — moved to the [backlog](../../stories/backlog/README.md).
- Running arbitrary non-Sonic titles well. They should *mostly* work, but no effort is spent on game-specific hacks for them.
- Linux. Not targeted, but nothing should actively prevent it (SDL3 + CMake).

## Users

A single technically skilled reverse engineer (the project owner), comfortable with 68k assembly, working
on one ROM at a time over many sessions. UI density beats discoverability.

## Target ROMs

| Game | Notable hardware use |
|------|----------------------|
| Sonic 1 | H-int for water palette (Labyrinth Zone), 68k-driven sound driver (SMPS 68k) with Z80 DAC driver, Nemesis/Enigma/Kosinski compression |
| Sonic 2 | Interlace mode 2 for 2P split-screen, Z80 SMPS driver (compressed), mid-frame palette changes (water), 1 MB ROM |
| Sonic 3 & Knuckles | 4 MB combined ROM (lock-on layout), battery SRAM via `$A130F1`, Z80 sound driver with DAC banking, 6-button pad support irrelevant but harmless |

ROMs are identified by **SHA-1** of the de-interleaved binary. Canonical revisions: see OPEN-QUESTIONS Q5.

## Success criteria by phase

1. **Emulator** (M00–M05): all three games play from power-on through at least the first act of every zone with correct video and audio at 60 fps on a 2020-era laptop.
2. **Workbench** (M06–M07): the user can freeze at any point, inspect all state, and see a live ROM map that classifies at least 95% of the bytes touched during a playthrough.
3. **Analysis** (M08): exported source reassembles to a ROM byte-identical to the original.
4. **Decompilation** (M09): for a sample of 50 functions, LLM-proposed names agree with the reference disassembly labels (exact or semantically equivalent) for ≥ 60%, measured blind.
5. **Modding** (M10): a modified Sonic 2 source tree with special-stage retry rebuilds, runs in the emulator with a coverage diff against the original, and runs on real hardware (flash cart).
6. **Splicing** (M11): the Sonic 1 special stage is playable inside Sonic 2, entered from a Sonic 2 special-stage ring, and awards a Sonic 2 emerald; then the same for S3K Blue Sphere.
7. **Hardware mod** (M12): the modded Sonic 2 ROM lights one emulated LED per emerald collected; the same ROM drives the physical cartridge LEDs.

## Glossary

| Term | Meaning |
|------|---------|
| 68k | Motorola 68000, main CPU, 7.67 MHz (NTSC) |
| Z80 | Zilog Z80, sound co-processor, 3.58 MHz (NTSC) |
| VDP | Video Display Processor (Yamaha YM7101) |
| FM / YM2612 | 6-channel FM synthesizer with a DAC on channel 6 |
| PSG | SN76489-compatible programmable sound generator built into the VDP |
| DMA | VDP transfer from 68k memory → VRAM/CRAM/VSRAM, or VRAM fill/copy |
| V-int / H-int | Vertical / horizontal blank interrupt (68k level 6 / level 4) |
| SMPS | Sample Music Playback System, Sega's sound driver used by Sonic games |
| SST | Sonic's object Status Table: per-object RAM structs |
| Coverage | Per-byte record of how memory was accessed during emulation |
| Watch session | A user-armed recording window: code first executed after arming is tagged as event-triggered |
| Layer | A named, saved coverage snapshot (e.g. "GHZ act 1", "after boss watch") |
| Reference labels | Labels imported from the Sonic Retro disassemblies |
| Master clock | 53.693175 MHz (NTSC) / 53.203424 MHz (PAL) system oscillator |
| Donor / target | In splicing: the game a subsystem is taken from (donor) and the game it's inserted into (target — Sonic 2) |
| Subsystem | A set of code, data, RAM, VRAM and sound resources that implements one game feature (e.g. a special stage) |
| Interface contract | Machine-checkable description of how a subsystem is entered and exited and which resources it uses |
| `/TIME` | Cartridge-slot strobe asserted for `$A13000–$A130FF`; used by cartridge-side registers |
