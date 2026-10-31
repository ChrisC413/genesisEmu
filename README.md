# genesisEmu

A Sega Genesis / Mega Drive emulator built as a **reverse-engineering workbench** for the Sonic games
(Sonic 1, Sonic 2, Sonic 3 & Knuckles). Playing the games is the means; the goal is to understand and
document their code well enough to produce **reassemblable, modifiable source**, and then to mod Sonic 2:

- modify its special stages and add **retry after failure**,
- **splice in special stages** from Sonic 1 and Sonic 3 & Knuckles,
- support a **cartridge hardware mod** that lights one LED per emerald collected.

All mods must run on real hardware. Targets: Windows 11 and macOS 15+.

This repository is **spec-driven**: specs describe *what* the system must do, stories describe
*incremental, testable slices* of work. Code follows the stories.

## Building

Needs CMake ≥ 3.25, vcpkg (`VCPKG_ROOT`) and MSVC 2022 or Apple clang:

```sh
cmake --preset win-debug && cmake --build --preset win-debug && ctest --preset win-debug   # Windows
cmake --preset mac-debug && cmake --build --preset mac-debug && ctest --preset mac-debug   # macOS
```

See [CONTRIBUTING.md](CONTRIBUTING.md) for formatting, CI and the macOS validation script.
Licensed under the [MIT License](LICENSE); third-party components are listed in
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

## Layout

```
docs/
  DECISIONS.md          Architecture decision log (ADRs, short form)
  OPEN-QUESTIONS.md     Unresolved questions — answer these before the milestone that needs them
  specs/                Technical specifications (the "what")
stories/
  M00-foundation/       One folder per milestone; each ends in a testable stage
  M01-m68k-cpu/
  ...                   Each story is its own file: M<milestone>-<NN>-<slug>.md
```

## Milestones

| #   | Milestone | Testable stage at exit |
|-----|-----------|------------------------|
| M00 | [Foundation](stories/M00-foundation/README.md) | App window opens on Windows + macOS, loads a ROM and shows its header; CI green |
| M01 | [68000 CPU](stories/M01-m68k-cpu/README.md) | 68000 core passes the SingleStepTests suite headless; disassembler verified |
| M02 | [System bus & boot](stories/M02-system-bus-boot/README.md) | Sonic 1/2/3K boot headless to their main loop, deterministically, with instruction traces |
| M03 | [VDP video](stories/M03-vdp-video/README.md) | SEGA logo, title screens and attract demos render correctly |
| M04 | [Input, Z80, playable](stories/M04-input-z80-playable/README.md) | All three games playable (silent) at full speed |
| M05 | [Audio](stories/M05-audio/README.md) | Music, SFX and DAC samples sound correct |
| M06 | [Debugger & freeze](stories/M06-debugger/README.md) | Freeze, step, breakpoints, inspectors, save states, trace-from-pause |
| M07 | [ROM exploration](stories/M07-rom-exploration/README.md) | Color-coded ROM map: code/data by CPU, frequency, every-frame, watch sessions |
| M08 | [Disassembly & analysis](stories/M08-disassembly-analysis/README.md) | Full disassembly that reassembles bit-identically to the original ROM |
| M09 | [LLM decompilation](stories/M09-llm-decompilation/README.md) | Select a function → pseudo-C, proposed names and a cross-checked interface contract → accept into symbol DB |
| M10 | [Modding pipeline & first S2 mods](stories/M10-modding-pipeline/README.md) | Sonic 2 *retry after failure* mod built from the app, diffed against the original, strict-mode clean, runs on a flash cart |
| M11 | [Special-stage splicing](stories/M11-special-stage-splicing/README.md) | Sonic 1 special stage (then Blue Sphere) playable inside Sonic 2, awarding S2 emeralds; equivalence harness passes |
| M12 | [Cartridge LED mod](stories/M12-cartridge-led-mod/README.md) | Modded S2 lights one emulated LED per emerald; the same ROM drives the physical cartridge |
| — | [Backlog](stories/backlog/README.md) | Unscheduled: C recompilation spike, rewind/movies, true lock-on |

M00–M05 build the emulator; M06–M09 build the workbench; M10–M12 deliver the mods. Parts of M06 (run
control, register and memory views) may start as soon as M02 is done — the debugger is invaluable while
bringing up the VDP and Z80. M12's emulator-side stories can run in parallel with M07–M10. Each milestone
README lists its dependencies.

```
M00 ─► M01 ─► M02 ─┬─► M03 ─► M04 ─► M05
                   └─► M06 ─┬─► M07 ─► M08 ─► M09 ─┬─► M10 ─► M11
                            │                      └─────────────┐
                            └─► M12 (emulator side) ──────────────┴─► M12 (ROM mod + hardware)
```

## Specs

| Spec | Topic |
|------|-------|
| [00-overview](docs/specs/00-overview.md) | Vision, goals, non-goals, glossary |
| [01-architecture](docs/specs/01-architecture.md) | Modules, scheduler, threading, instrumentation hooks, dependencies |
| [02-cpu-m68k](docs/specs/02-cpu-m68k.md) | Motorola 68000 core |
| [03-cpu-z80](docs/specs/03-cpu-z80.md) | Zilog Z80 core |
| [04-memory-bus](docs/specs/04-memory-bus.md) | Memory maps, I/O, cartridge, SRAM |
| [05-vdp](docs/specs/05-vdp.md) | Video Display Processor |
| [06-audio](docs/specs/06-audio.md) | YM2612, SN76489, mixing and sync |
| [07-frontend-platform](docs/specs/07-frontend-platform.md) | Windows/macOS app, windowing, input, config |
| [08-debugger](docs/specs/08-debugger.md) | Freeze/step, breakpoints, inspectors, save states, tracing |
| [09-coverage-and-rom-map](docs/specs/09-coverage-and-rom-map.md) | Execution/access coverage and the color-coded ROM map |
| [10-disassembler-and-analysis](docs/specs/10-disassembler-and-analysis.md) | Disassembly, code discovery, symbols, reassembly |
| [11-llm-decompilation](docs/specs/11-llm-decompilation.md) | LLM-assisted asm → high-level code with naming |
| [12-project-database](docs/specs/12-project-database.md) | Per-ROM project storage |
| [13-testing-strategy](docs/specs/13-testing-strategy.md) | Test suites, golden data, CI |
| [14-modding-pipeline](docs/specs/14-modding-pipeline.md) | Build/diff pipeline, hardware-strict mode, real-hardware validation, Sonic 2 special-stage mods |
| [15-subsystem-splicing](docs/specs/15-subsystem-splicing.md) | Extracting a special stage from a donor game and splicing it into Sonic 2 |
| [16-cartridge-hardware-mod](docs/specs/16-cartridge-hardware-mod.md) | Emerald-LED cartridge register interface, emulator device, hardware notes |

Also: [DECISIONS.md](docs/DECISIONS.md) and [OPEN-QUESTIONS.md](docs/OPEN-QUESTIONS.md).

## Story conventions

- File name: `M<milestone>-<NN>-<slug>.md`, e.g. `M01-03-effective-address-modes.md`.
- Every story has: **Story** (as a / I want / so that), **Scope**, **Acceptance criteria** (checkboxes),
  **Test plan**, **Depends on**, and **Spec refs**.
- Status values: `Draft` → `Ready` → `In progress` → `Done`. A story becomes `Ready` only when the open
  questions it depends on are resolved.
- A story is `Done` only when its acceptance criteria are met **and** its tests run in CI (Windows,
  GitHub Actions free tier). macOS is validated locally with `tools/validate-mac.sh`; tests needing
  commercial ROMs run locally (see [testing strategy](docs/specs/13-testing-strategy.md)).
- Stories are generated from `tools/gen_stories.py`; edit the data there and regenerate, or edit story
  files directly once work on them starts (then stop regenerating that milestone).

## ROMs

No ROMs are stored in this repository. The user supplies their own dumps. Tests that need commercial
ROMs look for them in the directory named by the `GENESIS_ROM_DIR` environment variable and are skipped
when it is absent.
