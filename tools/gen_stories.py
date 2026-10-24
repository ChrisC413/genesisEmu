"""Generates stories/<milestone>/*.md from the data below. Usage: python tools/gen_stories.py .  (from the repo root)"""
import sys, pathlib

ROOT = pathlib.Path(sys.argv[1])

def S(name, anchor=""):
    return f"[{name}](../../docs/specs/{name}.md{anchor})"

ARCH, M68K, Z80, BUS, VDP, AUD, FE, DBG, COV, DIS, LLM, DB, TEST, RECOMP = (
    "01-architecture", "02-cpu-m68k", "03-cpu-z80", "04-memory-bus", "05-vdp", "06-audio",
    "07-frontend-platform", "08-debugger", "09-coverage-and-rom-map", "10-disassembler-and-analysis",
    "11-llm-decompilation", "12-project-database", "13-testing-strategy", "14-modding-pipeline")
SPLICE, CART = "15-subsystem-splicing", "16-cartridge-hardware-mod"

M = []  # milestones

def milestone(folder, title, goal, stage, depends, notes="", prefix=None):
    m = dict(folder=folder, title=title, goal=goal, stage=stage, depends=depends, notes=notes, stories=[],
             prefix=prefix or folder.split("-")[0])
    M.append(m)
    return m

def story(m, slug, title, as_a, want, so_that, scope, ac, tests, deps=(), specs=(), notes=""):
    m["stories"].append(dict(slug=slug, title=title, as_a=as_a, want=want, so_that=so_that, scope=scope,
                             ac=ac, tests=tests, deps=list(deps), specs=list(specs), notes=notes))

DEV, RE, PLAYER = "developer", "reverse engineer", "player"

# ---------------------------------------------------------------- M00
m = milestone("M00-foundation", "Foundation",
    "Repository, build, CI and an application shell that loads a ROM — on both Windows and macOS.",
    "From a clean clone on Windows 11 and macOS 13+: build with one CMake preset, launch the app, open a ROM, see its parsed header and SHA-1. `gen_cli info --rom x` prints the same. GitHub Actions (Windows) is green and `tools/validate-mac.sh` passes locally.",
    "—")
story(m, "repo-and-build-system", "Repository layout and CMake/vcpkg build",
    DEV, "a CMake + vcpkg project skeleton with presets for Windows and macOS",
    "everyone builds the same way from a clean clone",
    ["Directory layout per architecture spec (`src/core`, `src/instrument`, `src/analysis`, `src/app`, `src/cli`, `tests`)",
     "`CMakePresets.json` with `win-debug/release`, `mac-debug/release`", "`vcpkg.json` manifest (SDL3, imgui[docking], sqlite3, nlohmann-json, curl, spdlog, xxhash, catch2)",
     "`.clang-format`, `.clang-tidy`, `.editorconfig`, `.gitignore` (ROMs, build dirs, test data)", "MIT `LICENSE` and `THIRD_PARTY_NOTICES.md` (D-014)", "Empty static libs and executables that link"],
    ["`cmake --preset win-debug && cmake --build --preset win-debug` succeeds on Windows 11 (MSVC 2022)",
     "`cmake --preset mac-debug && cmake --build --preset mac-debug` succeeds on macOS arm64",
     "C++20 enabled; warnings at `/W4` / `-Wall -Wextra`, treated as errors in CI",
     "A trivial Catch2 test runs via `ctest`"],
    ["Build on both platforms from a clean clone", "`ctest` passes"],
    specs=[S(ARCH, "#repository-layout"), S(FE, "#platforms")])
story(m, "ci-pipeline", "CI on GitHub Actions (Windows, free tier)",
    DEV, "every push built and tested on Windows in GitHub Actions using only free-tier features",
    "breakage is caught immediately without paid CI",
    ["Workflow on `windows-latest` (MSVC, x64), Debug + Release", "vcpkg binary caching via `actions/cache`", "clang-format check", "Upload Windows build artifact from `main`",
     "No macOS runners, larger runners or paid features (D-016)", "PR template with the macOS local-validation checklist (M00-07)"],
    ["Pushing a commit runs both Windows jobs", "A formatting violation fails the build", "A failing unit test fails the build", "Warm-cache build completes in < 10 minutes",
     "Workflow files contain no `runs-on` other than `windows-latest`"],
    ["Open a PR with a deliberate format error → CI fails; fix → passes"],
    deps=["M00-01"], specs=[S(TEST, "#ci")])
story(m, "logging-and-config", "Logging, configuration and platform paths",
    DEV, "structured logging and a JSON config stored in the right per-platform location",
    "all later subsystems have a consistent place for settings and diagnostics",
    ["spdlog sinks: console + rotating file; per-subsystem log levels", "Config load/save (JSON) with defaults and schema versioning",
     "Platform paths: `%APPDATA%\\genesisEmu` and `~/Library/Application Support/genesisEmu`", "Per-ROM data folder keyed by SHA-1"],
    ["Missing config → defaults written on first run", "Unknown keys preserved on save", "Log level per subsystem configurable from the config file", "Paths resolve correctly on both platforms"],
    ["Unit tests for config round-trip and migration", "Manual check of paths on both OSes"],
    deps=["M00-01"], specs=[S(FE, "#configuration-and-paths"), S(FE, "#logging")])
story(m, "rom-loading-and-header", "ROM loading and header parsing",
    RE, "to open any Genesis ROM dump and see its identity and header",
    "I always know exactly which ROM revision I'm analyzing",
    ["Load `.bin/.md/.gen/.smd`; detect and de-interleave SMD", "Parse header `$100–$1FF` (titles, serial, checksum, region, ROM/RAM ranges, SRAM declaration)",
     "Compute header checksum, SHA-1 and CRC32", "Known-ROM table: SHA-1 → game + revision for the target games"],
    ["Interleaved SMD and plain BIN of the same game yield the same SHA-1", "Header checksum mismatch is reported but doesn't block loading",
     "Target-game ROMs are identified by name and revision", "Files > 4 MB or < 512 bytes are rejected with a clear error"],
    ["Unit tests with synthetic ROMs (generated in-test) for header parsing and SMD de-interleave", "`[rom]` test identifies the user's Sonic ROMs"],
    deps=["M00-01"], specs=[S(BUS, "#cartridge-loading")], notes="Q5 decides which revisions are canonical.")
story(m, "app-shell-window", "Application shell: SDL3 window with ImGui docking",
    RE, "a desktop window with a docking layout, menus and an empty game view",
    "every later panel has a home",
    ["SDL3 window + renderer backend for ImGui (OpenGL 3.3 or SDL_GPU), Hi-DPI", "Docking layout persisted to config", "Main menu skeleton (File/Emulation/Debug/Analysis/Help)",
     "File → Open ROM via native dialog, Recent ROMs list", "ROM info panel showing parsed header (M00-04)", "macOS `.app` bundle; Windows exe with icon"],
    ["App launches on both platforms and renders at the display refresh rate", "Panels can be docked/undocked; layout restored on restart",
     "Opening a ROM populates the ROM info panel", "Window is crisp on Retina / 150% scaling displays"],
    ["Manual smoke test on both OSes", "Screenshot attached to the PR"],
    deps=["M00-01", "M00-03", "M00-04"], specs=[S(FE, "#application-shell")])
story(m, "headless-cli-skeleton", "Headless CLI runner skeleton",
    DEV, "a `gen_cli` executable with subcommands for info, run and test",
    "automation and CI can drive the core without a UI",
    ["Argument parsing; subcommands `info`, `run --frames N`, later `trace`, `coverage`, `export`", "Exit codes: 0 ok, 1 usage, 2 load error, 3 check failure"],
    ["`gen_cli info --rom x.bin` prints header, SHA-1, identified game", "`gen_cli run` exists (no-op until M02) and returns 0", "`--help` documents every subcommand"],
    ["CTest invokes `gen_cli info` on a synthetic ROM"],
    deps=["M00-04"], specs=[S(ARCH, "#modules")])
story(m, "local-mac-validation", "Local macOS validation script",
    DEV, "a single script that builds and tests the macOS app locally and prints a PR-ready summary",
    "macOS stays healthy without paying for macOS CI runners",
    ["`tools/validate-mac.sh`: configure + build `mac-release`, run the same tests CI runs, ASan/UBSan build + tests, create the `.app`",
     "Summary block (commit, macOS version, arch, pass/fail counts, duration) for the PR", "`tools/validate-roms.(sh|ps1)` for local `[rom]` tests on either OS",
     "Documented in `CONTRIBUTING.md`: when the macOS check is required (platform code, CMake, audio/video backends)"],
    ["Script runs from a clean clone on macOS 13+ with only Xcode CLT, CMake and vcpkg installed", "Non-zero exit on any failure", "Summary block pasted into the PR template matches the template's fields"],
    ["Run on an Apple Silicon Mac; introduce a failing test and confirm a non-zero exit"],
    deps=["M00-01", "M00-02"], specs=[S(TEST, "#ci"), S(FE, "#platforms")])

# ---------------------------------------------------------------- M01
m = milestone("M01-m68k-cpu", "Motorola 68000 CPU",
    "A hand-written, instruction-by-instruction 68000 core with exact cycle counts, plus a 68k disassembler.",
    "Headless test binary runs the full SingleStepTests 68000 suite: 100% pass (or an explicit, justified exclusion list), including cycle counts. Disassembler round-trips through AS for every valid opcode pattern.",
    "M00")
story(m, "cpu-core-framework", "68000 core framework: registers, bus interface, decode table",
    DEV, "the CPU skeleton — register file, bus interface, opcode decode table and `step()` loop",
    "instruction groups can be added incrementally and tested in isolation",
    ["`M68kState` struct (D0–D7, A0–A7, USP, SSP, PC, SR, IR, IRC, stopped)", "`Bus68k` interface (`read8/16/32`, `write8/16/32`, `iack`)",
     "65,536-entry decode table built from pattern strings; unimplemented → illegal handler", "`step()` returns cycles; `onInstr` and `onAccess` hook calls (spec 01)", "Supervisor/user stack switching on SR.S change"],
    ["Decode table construction takes < 50 ms", "Hooks fire with correct PC for every access", "An unimplemented opcode raises the illegal-instruction exception path (stubbed)"],
    ["Unit tests with a flat 16 MB test bus"],
    specs=[S(M68K, "#implementation-approach"), S(ARCH, "#instrumentation-hook-api")])
story(m, "singlesteptests-harness", "SingleStepTests 68000 harness",
    DEV, "a test runner that loads the SingleStepTests JSON vectors and compares full CPU + memory state",
    "every instruction group is verified the moment it's written",
    ["`tools/fetch-test-data` downloads the suite pinned by commit + checksum", "Runner: set initial state (regs, prefetch, RAM), run one instruction, compare final regs/RAM/cycles",
     "Filter by mnemonic file; summary table of pass/fail per instruction", "Exclusion list file with a reason per entry"],
    ["Runner reports per-file pass rates", "A mismatch prints initial state, expected vs actual diff", "CI runs a fast subset per push; nightly runs all"],
    ["Self-test: a deliberately broken NOP fails the NOP vectors"],
    deps=["M01-01"], specs=[S(M68K), S(TEST, "#layers")])
story(m, "effective-address-modes", "Effective address modes",
    DEV, "all 14 68000 addressing modes with correct side effects and EA timing",
    "every instruction shares one correct operand path",
    ["Dn, An, (An), (An)+, -(An), d16(An), d8(An,Xn), abs.W, abs.L, d16(PC), d8(PC,Xn), #imm", "A7 byte post-inc/pre-dec by 2", "EA calculation cycle table", "Address error detection on odd word/long access (exception processing in M01-10)"],
    ["Operand fetch/store templated on size and mode", "Index register sign-extension (Xn.W vs Xn.L) correct", "EA cycle table matches the User's Manual"],
    ["Pass SingleStepTests for MOVE.x (exercises all EA combinations)"],
    deps=["M01-01", "M01-02"], specs=[S(M68K, "#requirements")])
story(m, "data-movement", "Data movement instructions",
    DEV, "MOVE, MOVEA, MOVEQ, MOVEM, MOVEP, LEA, PEA, EXG, SWAP, LINK, UNLK, CLR",
    "the core can shuffle data like real code does",
    ["All listed instructions and sizes", "MOVEM register-list order and pre-decrement reversal; MOVEM.W sign-extension into An", "MOVEP byte-lane behavior"],
    ["100% pass of the corresponding SingleStepTests files"],
    ["SingleStepTests for each mnemonic"],
    deps=["M01-03"], specs=[S(M68K, "#instruction-groups-story-breakdown")])
story(m, "integer-arithmetic", "Integer arithmetic instructions",
    DEV, "ADD/SUB families, CMP family, MULU/MULS, DIVU/DIVS, NEG/NEGX, EXT",
    "game math and comparisons are exact",
    ["X-flag semantics for ADDX/SUBX/NEGX (Z only cleared, never set)", "Data-dependent MUL/DIV timing", "DIV overflow behavior and zero-divide exception hook"],
    ["100% pass of corresponding SingleStepTests including cycle counts"],
    ["SingleStepTests per mnemonic"],
    deps=["M01-03"], specs=[S(M68K, "#cycle-timing-notes")])
story(m, "logic-shift-bit", "Logic, shift/rotate and bit instructions",
    DEV, "AND/OR/EOR/NOT (+ immediates), all shifts and rotates, BTST/BSET/BCLR/BCHG, TST, TAS, Scc",
    "bit-twiddling code (flags, collision masks) behaves correctly",
    ["Register and memory forms of shifts/rotates; count modulo 64", "Bit ops: modulo 32 for Dn, modulo 8 for memory", "TAS with Genesis write-back quirk option (C68-9)"],
    ["100% pass of corresponding SingleStepTests"],
    ["SingleStepTests per mnemonic", "Unit test for TAS quirk toggle"],
    deps=["M01-03"], specs=[S(M68K, "#requirements")])
story(m, "bcd-instructions", "BCD instructions",
    DEV, "ABCD, SBCD, NBCD including undefined-flag behavior as the test suite expects",
    "score/timer display code is correct",
    ["Register and -(An) forms", "N and V flag behavior per real silicon"],
    ["100% pass of SingleStepTests ABCD/SBCD/NBCD"],
    ["SingleStepTests"], deps=["M01-03"], specs=[S(M68K)])
story(m, "program-control", "Program control instructions",
    DEV, "Bcc, BRA, BSR, DBcc, JMP, JSR, RTS, RTR, RTE, NOP",
    "control flow — and the debugger's step-over logic — works",
    ["Short/word branch displacements; taken/not-taken timing", "DBcc counter wrap at -1", "RTE format for 68000 (SR + PC)"],
    ["100% pass of corresponding SingleStepTests", "Flow type (call/return/branch/jump) exposed for each executed instruction (used by the debugger and coverage)"],
    ["SingleStepTests"], deps=["M01-03"], specs=[S(M68K)])
story(m, "system-instructions", "System and status-register instructions",
    DEV, "MOVE to/from SR/CCR, MOVE USP, ANDI/ORI/EORI to CCR/SR, STOP, RESET",
    "supervisor code and interrupt masking work",
    ["Privilege violation on user-mode SR writes", "Stack pointer swap on S-bit change", "STOP waits for interrupt; RESET reported via hook"],
    ["100% pass of corresponding SingleStepTests"],
    ["SingleStepTests", "Unit test: STOP resumes on an interrupt above mask"],
    deps=["M01-03"], specs=[S(M68K)])
story(m, "exceptions-and-interrupts", "Exceptions and interrupts",
    DEV, "the full 68000 exception model: reset, address error, illegal, line-A/F, privilege, trace, TRAP, TRAPV, CHK, zero divide, autovectored interrupts",
    "the CPU can run real software and the debugger can break on exceptions",
    ["Exception stack frames (group 0 long frame for address error)", "Interrupt sampling between instructions, level 7 non-maskable, IPL mask update", "Autovector IACK cycles; spurious interrupt",
     "Trace exception after each instruction when T=1", "`onIrq` hook"],
    ["100% pass of SingleStepTests TRAP/CHK/TRAPV/ILLEGAL/address-error cases", "Unit test: level-6 interrupt with mask 3 vectors to `$78`; with mask 6 it waits",
     "Reset reads SSP from `$0` and PC from `$4`"],
    ["SingleStepTests", "Interrupt unit tests"],
    deps=["M01-04", "M01-05", "M01-09"], specs=[S(M68K, "#requirements")])
story(m, "cycle-timing-audit", "Cycle timing audit",
    DEV, "every instruction's cycle count verified against the suite and the manual tables",
    "frame timing — and therefore V-int, H-int and DMA scheduling — is right",
    ["Cross-check suite cycle fields for all files", "Document any deliberate deviation"],
    ["Cycle mismatches across the full suite = 0 (or listed exclusions)", "Throughput benchmark: ≥ 100 MIPS-equivalent of 68k instructions/sec on the reference machine (≫ 1.3 M/s needed)"],
    ["Full suite run with cycle checking", "Microbenchmark in `tests/bench`"],
    deps=["M01-04", "M01-05", "M01-06", "M01-07", "M01-08", "M01-09", "M01-10"], specs=[S(M68K, "#cycle-timing-notes")])
story(m, "m68k-disassembler", "68000 disassembler (AS syntax)",
    RE, "a 68000 disassembler producing AS-compatible Motorola syntax with flow information",
    "the debugger, tracer and analysis tools all share one correct disassembly",
    ["Library in `gen_analysis/disasm68k`", "Output: text, length, flow type, branch/jump targets, referenced absolute addresses", "Symbol resolver callback for operand naming", "Illegal patterns → `dc.w $XXXX`"],
    ["Every opcode the core decodes is disassembled", "Round-trip test: generated instruction corpus → disassemble → assemble with AS → identical bytes", "Explicit sizes always emitted"],
    ["Round-trip corpus test (AS fetched by the test-data script)", "Golden text tests for tricky encodings (MOVEM lists, PC-relative, BCD)"],
    deps=["M01-01"], specs=[S(M68K, "#disassembler-lives-in-gen_analysis-but-delivered-in-m01"), S(DIS)],
    notes="Can be developed in parallel with the instruction groups.")

# ---------------------------------------------------------------- M02
m = milestone("M02-system-bus-boot", "System bus & boot",
    "Memory map, I/O, scheduler and VDP/Z80 stubs so real games boot headless; instruction tracing for bring-up.",
    "`gen_cli run --rom <sonic> --frames 600` completes for Sonic 1, 2 and 3K with no unexpected exceptions, reaching each game's V-int wait loop; two runs produce identical state hashes; `gen_cli trace` writes an instruction trace.",
    "M01")
story(m, "memory-map-and-bus", "68k memory map and bus",
    DEV, "the 68k bus decoding ROM, RAM, I/O, Z80 window and VDP ranges with mirrors and open bus",
    "games read and write the right places",
    ["Page table (64 KB pages) with fast paths for ROM/RAM", "Mirrors for RAM and VDP ports", "Open-bus reads, unmapped-access log", "Fetch vs data-read distinction in `onAccess`"],
    ["Unit tests for every range in the spec 04 table", "RAM mirror `$E00000` aliases `$FF0000`", "Unmapped writes logged with PC"],
    ["Unit tests with a synthetic ROM"],
    specs=[S(BUS, "#68000-memory-map"), S(BUS, "#bus-requirements")])
story(m, "io-and-version-registers", "I/O, version and TMSS registers",
    DEV, "version register, controller port registers (idle), and TMSS",
    "boot code's region checks and port init pass",
    ["Version register from config/header region", "Controller data/control registers (no input yet: idle values)", "TMSS optional (default off)"],
    ["Sonic 1 region check passes for its native region", "Reading port 1 returns idle (`$7F`-style) values with TH high"],
    ["Unit tests; boot test in M02-07"],
    deps=["M02-01"], specs=[S(BUS, "#io-registers")])
story(m, "z80-bus-control-stub", "Z80 bus request / reset and Z80 RAM access",
    DEV, "BUSREQ/RESET registers and Z80 RAM accessible from the 68k (Z80 not yet executing)",
    "games can load their sound drivers during boot",
    ["`$A11100` / `$A11200` semantics", "68k access to `$A00000–$A0FFFF` when bus owned", "Z80 RAM kept in core state (8 KB)"],
    ["Bus grant reads back correctly", "Sonic 1 copies its DAC driver into Z80 RAM (bytes visible in state)", "Access without owning the bus is logged"],
    ["Unit tests"], deps=["M02-01"], specs=[S(BUS, "#z80-memory-map")])
story(m, "vdp-port-stub", "VDP port stub: status, HV counter, V-int",
    DEV, "a VDP stub that accepts commands, stores memories, reports status and raises V-int",
    "boot code that polls the VDP doesn't hang",
    ["Control/data port command latch, register writes, VRAM/CRAM/VSRAM writes (no rendering)", "Status register V-blank/H-blank bits from scanline position", "V-int at line 224 when enabled"],
    ["Sonic 1 V-int handler executes once per frame", "VRAM contents after boot are plausible (non-zero tiles)"],
    ["Unit tests for command parsing", "Boot test"],
    deps=["M02-01", "M02-05"], specs=[S(VDP, "#ports")], notes="This grows into the full VDP in M03-01.")
story(m, "master-clock-scheduler", "Master-clock scheduler (NTSC/PAL)",
    DEV, "a scanline scheduler driving the 68k (and later Z80/audio) by master clock",
    "all components stay in sync deterministically",
    ["64-bit MCLK counter; 3420 MCLK/line; 262/313 lines", "68k runs per scanline with overshoot carry", "Catch-up interface for Z80 and audio (no-ops for now)", "`Machine::run(RunLimit)` / `step(cpu)` and `RunResult`"],
    ["One NTSC frame = 896,040 MCLK", "Frame counter and line counter exposed", "`run(frames=1)` stops exactly at frame end"],
    ["Unit tests with a timing test ROM (synthetic)"],
    deps=["M02-01"], specs=[S(ARCH, "#timing-and-scheduler")])
story(m, "instruction-trace-logger", "Instruction trace logger (CLI)",
    DEV, "`gen_cli trace` writing a text trace of 68k instructions with register deltas",
    "I can diff bring-up behavior against a reference emulator",
    ["Text format per spec 08 tracing section", "Frame range / PC range filters", "`tools/trace-diff` reporting first divergence"],
    ["Trace of frame 0–10 of Sonic 1 is produced", "Format documented in `docs/specs/08-debugger.md` (trace section)", "trace-diff finds an injected divergence"],
    ["Golden trace for a synthetic ROM"],
    deps=["M01-12", "M02-05"], specs=[S(DBG, "#tracing-trace-from-pause-d-007"), S(TEST, "#reference-comparison-debug-aid")],
    notes="The interactive trace-from-pause UI comes in M06-09 and reuses this writer.")
story(m, "boot-smoke-tests", "Boot smoke tests and determinism",
    DEV, "automated boot tests for all three games plus a determinism check",
    "regressions in CPU/bus/scheduler are caught immediately",
    ["`[rom]` tests: run 600 frames per game; assert no illegal/address-error exceptions; assert PC is in the V-int wait loop at frame end", "State hash (xxHash of all component state)", "Run twice → same hash"],
    ["All three games pass locally with `GENESIS_ROM_DIR` set", "Tests skip cleanly without ROMs", "State hash is identical on Windows and macOS"],
    ["CTest `[rom]` label"],
    deps=["M02-02", "M02-03", "M02-04", "M02-05"], specs=[S(TEST, "#layers")])

# ---------------------------------------------------------------- M03
m = milestone("M03-vdp-video", "VDP video",
    "A complete scanline VDP: planes, scrolling, window, sprites, priority, shadow/highlight, DMA, interrupts, interlace mode 2 — presented in the app.",
    "In the app, Sonic 1/2/3K show the SEGA logo, title screen and attract-mode demos with correct graphics, scrolling, water palettes and sprites. Golden frame hashes are recorded for these checkpoints.",
    "M02", notes="Input is not yet available: rely on attract demos and scripted state. Debug viewers (M06-07) help a lot here and may be pulled forward.")
story(m, "vdp-ports-registers-memories", "VDP ports, registers and memories",
    DEV, "the full VDP command interface with hardware-accurate latch behavior",
    "data lands in VRAM/CRAM/VSRAM exactly as on hardware",
    ["All 24 registers decoded", "Command word latch rules; read/write codes; auto-increment", "Data-port reads with read-ahead buffer", "Byte-write duplication", "Status register incl. FIFO approximation"],
    ["Unit tests for every command code and latch edge case", "CRAM/VSRAM wraparound correct"],
    ["Unit tests; free VDP test ROMs where available"],
    specs=[S(VDP, "#ports"), S(VDP, "#memories")])
story(m, "dma", "VDP DMA: 68k transfer, fill, copy",
    DEV, "all three DMA types with 68k stall approximation and `onDma` events",
    "graphics/palette uploads work and the ROM map can classify DMA sources",
    ["68k→VRAM/CRAM/VSRAM; source/length register update after transfer", "Fill and copy", "68k freeze duration from bandwidth table", "`onDma` hook with source/dest/length"],
    ["Sonic 1 title screen art arrives in VRAM via DMA", "Source address wrap at 128 KB boundary handled", "Hook events match DMAs issued"],
    ["Unit tests; DMA test ROM"],
    deps=["M03-01"], specs=[S(VDP, "#dma")])
story(m, "interrupts-and-counters", "H-int, V-int and HV counter",
    DEV, "accurate per-line interrupt timing and HV counter values",
    "water effects and raster tricks happen on the correct lines",
    ["H-int counter reload/decrement", "V-int pending flag, acknowledge", "HV counter tables for H32/H40, V28/V30, interlace", "Z80 /INT assertion for one line"],
    ["Labyrinth Zone water line in Sonic 1 appears at the right height", "Unit tests for counter sequences"],
    ["Unit tests; visual check"], deps=["M03-01"], specs=[S(VDP, "#interrupts-and-timing")])
story(m, "plane-rendering", "Plane A/B rendering",
    DEV, "nametable-based plane rendering for H32/H40 with all plane sizes",
    "backgrounds and foregrounds appear",
    ["Tile decode (4bpp, flip H/V, palette, priority)", "Plane sizes 32/64/128, mirroring for invalid combos", "Background color, display enable, left-column blank"],
    ["SEGA logo and Sonic 1 title background render correctly", "Golden frame hash for Sonic 1 title"],
    ["Golden-frame `[rom]` test"], deps=["M03-01"], specs=[S(VDP, "#rendering")])
story(m, "scrolling", "Horizontal and vertical scrolling",
    DEV, "all H-scroll (full/cell/line) and V-scroll (full/2-cell) modes",
    "parallax and level scrolling are right",
    ["H-scroll table fetch per line", "VSRAM per-column scroll", "Leftmost-column V-scroll quirk (approximate)"],
    ["Sonic 1 GHZ parallax correct in attract demo", "Sonic 2 EHZ water ripple line scroll correct"],
    ["Golden-frame tests"], deps=["M03-04"], specs=[S(VDP, "#rendering")])
story(m, "window-plane", "Window plane",
    DEV, "the window plane replacing plane A in the configured region",
    "HUD-style fixed layers render correctly",
    ["Regs 17/18 left/right/up/down regions", "Window ignores scroll", "Window bug (H-scroll interaction) approximated"],
    ["Test ROM window regions render correctly", "Sonic 2 2P / menus using window look right"],
    ["Test ROM golden frame"], deps=["M03-04"], specs=[S(VDP, "#rendering")])
story(m, "sprites", "Sprites",
    DEV, "sprite rendering with link list, per-line limits, masking and status flags",
    "Sonic, rings, enemies and HUD appear correctly",
    ["SAT parse (cached sprite table), sizes 1×1–4×4 cells", "Per-line sprite/pixel limits by mode", "Masking at x=0 rule", "Overflow and collision status bits"],
    ["Attract demo sprites correct in all three games", "Sprite overflow behaves on a test ROM"],
    ["Golden frames; test ROM"], deps=["M03-04"], specs=[S(VDP, "#rendering")])
story(m, "priority-and-shadow-highlight", "Layer priority and shadow/highlight",
    DEV, "per-pixel priority composition and shadow/highlight mode",
    "sprites go behind/in front of terrain correctly and S/H effects work",
    ["Six-level priority composition", "Shadow/highlight with palette 3 color 14/15 operators"],
    ["Sonic loop-de-loops (priority swap) correct", "S/H test ROM matches golden"],
    ["Golden frames"], deps=["M03-05", "M03-07"], specs=[S(VDP, "#rendering")])
story(m, "interlace-mode-2", "Interlace mode 2 (double resolution)",
    DEV, "interlace mode 2 rendering with 8×16 tiles and 448-line output",
    "Sonic 2's 2-player split-screen works",
    ["Tile addressing for 8×16 cells", "Field alternation and odd-frame status bit", "Output 320×448 framebuffer (or weaving option)"],
    ["Sonic 2 2P mode (reached via save state or scripted input later) renders both halves correctly"],
    ["Golden frame once input exists (M04)"], deps=["M03-07"], specs=[S(VDP, "#display-modes")])
story(m, "frame-presentation", "Frame presentation in the app",
    PLAYER, "the emulated framebuffer shown in the game view with scaling and correct pacing",
    "I can watch the game run smoothly",
    ["Framebuffer → GPU texture each frame", "Integer scale / fit, aspect correction, filtering options", "Frame pacing at 59.92 Hz with vsync; fast-forward", "Run/Pause/Reset menu wired to the core"],
    ["No tearing; steady frame pacing on a 60 Hz display", "Resizing window keeps aspect", "Fast-forward runs ≥ 5× on the reference machine"],
    ["Manual check on both OSes"], deps=["M03-04"], specs=[S(FE, "#frame-pacing"), S(FE, "#application-shell")])
story(m, "video-regression-tests", "Video regression tests",
    DEV, "golden frame hashes at fixed frames of each game's boot/attract sequence",
    "VDP regressions are caught",
    ["`gen_cli run --check` with checkpoint frame hashes", "Golden file management per spec 13", "Diff image output on mismatch (expected/actual/diff PNG)"],
    ["≥ 5 checkpoints per game", "Mismatch produces diff images"],
    ["`[rom]` CTest"], deps=["M03-08"], specs=[S(TEST, "#test-data-policy")])

# ---------------------------------------------------------------- M04
m = milestone("M04-input-z80-playable", "Input, Z80, playable",
    "Controllers, the hand-written Z80 core and its system integration, SRAM — making all three games playable (without sound).",
    "A human can play Sonic 1 GHZ1, Sonic 2 EHZ1 and S3K AIZ1 start to finish at full speed using keyboard or gamepad; the Z80 core passes ZEXALL and SingleStepTests; S3K saves persist across restarts.",
    "M03")
story(m, "controller-io", "Controller port protocol (3- and 6-button)",
    DEV, "the TH-line controller protocol for 3- and 6-button pads",
    "games read input correctly",
    ["TH select multiplexing; 6-button extra cycles with timeout reset", "Per-port pad type from config", "Input latched once per frame from the host (determinism)"],
    ["Unit tests replay the read sequence games use", "Sonic games detect a 3-button pad"],
    ["Unit tests"], specs=[S(BUS, "#io-registers"), S(FE, "#input")])
story(m, "input-mapping", "Keyboard/gamepad mapping and rebinding",
    PLAYER, "keyboard and gamepad support with rebinding",
    "I can play comfortably",
    ["SDL3 gamepad hot-plug", "Default keymap per spec 07", "Rebinding UI with capture", "Hotkey table (pause, step, save/load state) — handlers stubbed until M06"],
    ["Plug/unplug a gamepad while running without crash", "Bindings persist across restarts"],
    ["Manual test with Xbox and PlayStation controllers"], deps=["M04-01"], specs=[S(FE, "#input")])
story(m, "scripted-input-for-tests", "Scripted input for tests",
    DEV, "the minimal scripted-input file format in `gen_cli`",
    "regression tests can play games deterministically",
    ["Parser for the format in spec 13", "Checkpoints for frame and state hashes", "Not a user-facing movie feature (D-007)"],
    ["Script that starts Sonic 1 and walks right reaches the same frame hash every run on both OSes"],
    ["`[rom]` CTest"], deps=["M04-01"], specs=[S(TEST, "#scripted-input-format-tests-only")])
story(m, "z80-core-and-harness", "Z80 core framework and test harnesses",
    DEV, "the Z80 skeleton plus ZEXDOC/ZEXALL and SingleStepTests runners",
    "Z80 instructions are verified as they're written",
    ["`Z80State`, `BusZ80`, prefix decode tables, `step()` returning T-states, hooks", "CP/M BDOS shim for ZEX", "SingleStepTests JSON runner (shares code with M01-02)"],
    ["Runners execute and report per-opcode results"],
    ["Harness self-test"], specs=[S(Z80, "#implementation-approach")])
story(m, "z80-instruction-set", "Z80 instruction set (documented + undocumented)",
    DEV, "every Z80 opcode including undocumented ones and X/Y flags",
    "the sound drivers run exactly as on hardware",
    ["Unprefixed, CB, ED, DD/FD, DDCB/FDCB", "Block instructions and repeat timing", "R, I, IFF, IM 0/1/2, HALT, EI delay, WZ"],
    ["ZEXDOC and ZEXALL pass 100%", "SingleStepTests Z80 pass 100% (or justified exclusions)"],
    ["Nightly full suites"], deps=["M04-04"], specs=[S(Z80, "#requirements")])
story(m, "z80-disassembler", "Z80 disassembler (AS syntax)",
    RE, "a Z80 disassembler sharing the 68k disassembler's output structure",
    "sound-driver code is readable in the debugger and analysis tools",
    ["All documented + undocumented opcodes", "Flow info and targets"],
    ["Round-trip through AS for all opcodes"],
    ["Corpus round-trip test"], deps=["M04-04"], specs=[S(Z80, "#disassembler")])
story(m, "z80-system-integration", "Z80 system integration",
    DEV, "the Z80 running in the scheduler with bus arbitration, bank window, interrupts and sound-chip ports",
    "games' Z80 drivers run and communicate with the 68k",
    ["Catch-up execution per scanline", "BUSREQ/RESET halting", "Bank register serial writes; 68k window with stall penalties and translated-address reporting (CZ-6)", "V-blank /INT for one line", "YM2612/PSG port writes forwarded to stubs (register log)"],
    ["Sonic 2/S3K Z80 drivers run without hanging the 68k (handshakes complete)", "Register-write log shows plausible YM writes during title music"],
    ["`[rom]` boot tests extended", "Unit tests for bank register"], deps=["M04-05"], specs=[S(Z80, "#requirements"), S(BUS, "#z80-memory-map")])
story(m, "sram-and-cart-devices", "Cartridge device chain, SRAM and `$A130F1`",
    PLAYER, "battery saves for Sonic 3 & Knuckles, built on a pluggable cartridge-device chain",
    "my save files persist and custom cartridge hardware (the emerald-LED mod) can be added later",
    ["`ICartDevice` chain claiming `/TIME` (`$A130xx`) addresses and optional ROM-space write snooping; devices in save states", "SRAM controller as the first device",
     "Odd-byte SRAM at `$200001+` per header", "`$A130F1` enable/write-protect switching ROM/SRAM", "Persist `<sha1>.srm` on exit and periodically when dirty"],
    ["S3K save slot survives app restart", "Disabling SRAM restores ROM visibility at `$200000`", "A test device claiming `$A130E1` receives writes; unclaimed `$A130xx` writes are logged"],
    ["Unit tests; manual S3K save test"], specs=[S(BUS, "#cartridge-device-chain"), S(BUS, "#cartridge-loading")])
story(m, "playability-milestone", "Playability milestone",
    PLAYER, "to play the first act of each target game at full speed",
    "the emulator core is proven before building the workbench on top",
    ["Performance profiling and fixes", "Bug triage list of visual issues found while playing"],
    ["GHZ1, EHZ1, AIZ1 completed by a human without visible glitches", "≤ 25% of one core used at 60 fps on the reference machine (no instrumentation)", "Scripted-input regression for each act start"],
    ["Manual playtest checklist in the PR", "Benchmark results recorded"],
    deps=["M04-02", "M04-03", "M04-07", "M04-08"], specs=[S("00-overview", "#success-criteria-by-phase")])

# ---------------------------------------------------------------- M05
m = milestone("M05-audio", "Audio",
    "PSG and YM2612 synthesis, mixing, resampling and audio/video sync, plus analysis aids (mute/solo, register log).",
    "All three games' music, SFX and DAC samples (incl. the \"SEGA\" voice) sound correct at 48 kHz without crackles or drift over a 30-minute session; per-channel mute works; register log exports CSV.",
    "M04", notes="YM2612 uses ymfm (D-013); the PSG is in-house.")
story(m, "audio-output-and-sync", "Audio output, resampling and A/V sync",
    PLAYER, "glitch-free audio synchronized with video",
    "the game sounds right for long sessions",
    ["SDL_AudioStream output at 48 kHz", "Band-limited resampler", "Dynamic rate control ±0.5% around a fill target", "Silence when paused; mute on fast-forward"],
    ["No underruns over 30 minutes at 60 Hz and at 144 Hz host refresh", "Pause produces silence immediately"],
    ["Long-run soak test with fill-level logging"], specs=[S(AUD, "#mixing-and-output")])
story(m, "psg-sn76489", "PSG (SN76489)",
    DEV, "an in-house PSG implementation",
    "PSG music channels and SFX are correct",
    ["3 tone + noise, Sega LFSR taps, attenuation table", "Writes from 68k and Z80", "Catch-up stepping"],
    ["Unit tests: tone period, LFSR sequence, volume table", "Sonic 1 ring SFX PSG component sounds right"],
    ["Unit tests"], deps=["M05-01"], specs=[S(AUD, "#sn76489-psg")])
story(m, "ym2612-fm", "YM2612 FM synthesis (ymfm)",
    DEV, "YM2612 FM synthesis using ymfm behind an `IFmSynth` interface",
    "music sounds right without writing an FM core from scratch",
    ["Integrate `ymfm::ym2612` (vcpkg or vendored in `third_party/` with license notice)", "`IFmSynth`: write/read_status/generate/serialize", "Register port interface (A0/D0/A1/D1), busy flag", "Catch-up clocking at MCLK/7"],
    ["Sonic 1/2/3K title music matches reference recordings by ear and within tolerance in the WAV comparison", "FM state round-trips through save states", "ymfm license listed in `THIRD_PARTY_NOTICES.md`"],
    ["WAV comparison tests", "Save-state round-trip test"], deps=["M05-01"], specs=[S(AUD, "#ym2612-fm")])
story(m, "ym2612-dac-and-timers", "YM2612 DAC and timers",
    DEV, "DAC playback on channel 6 and timers A/B",
    "drums, voice clips and timer-driven drivers work",
    ["DAC enable/data", "Timer A/B overflow flags and reload", "Accurate-enough DAC timing from Z80 write cadence"],
    ["\"SEGA\" voice in Sonic 1 clear and correct pitch", "S3K DAC drums correct"],
    ["Manual listening + WAV test"], deps=["M05-03"], specs=[S(AUD, "#ym2612-fm")])
story(m, "mixer-mute-and-register-log", "Mixer, channel mute/solo and register-write log",
    RE, "per-channel mute/solo and a log of every sound-chip register write with PC",
    "I can reverse engineer the SMPS drivers",
    ["FM/PSG balance and low-pass options", "Mute/solo per channel", "Ring buffer of `(frame, line, cpu, pc, chip, port, reg, value)`; CSV export", "Audio panel in the UI"],
    ["Muting FM6 silences DAC", "Log export of 10 s of title music contains expected key-on writes with Z80 PCs"],
    ["Unit tests for log; manual UI test"], deps=["M05-02", "M05-04"], specs=[S(AUD, "#analysis-features")])
story(m, "audio-regression-tests", "Audio regression tests and WAV recording",
    DEV, "WAV recording and audio-hash checkpoints in scripted tests",
    "audio regressions are caught",
    ["WAV writer of mixed output", "Audio hash checkpoint in the script format"],
    ["Title-screen audio hash stable across runs and OSes"],
    ["`[rom]` CTest"], deps=["M05-05", "M04-03"], specs=[S(AUD, "#testing")])

# ---------------------------------------------------------------- M06
m = milestone("M06-debugger", "Debugger & freeze execution",
    "Freeze at any instruction, step, break on anything, inspect and edit all state, save/load states, and trace from a paused state.",
    "In the app: pause Sonic mid-level, step into/over/out on the 68k and Z80, set exec/memory/VDP breakpoints with conditions, inspect every memory and VDP structure, save and reload a state (determinism check passes), and record a trace started from the paused state.",
    "M02 (core stories M06-01..06, 08, 09, 10), M03/M04/M05 for the VDP/Z80/audio panels",
    notes="Start M06-01, M06-02, M06-05, M06-06 right after M02 to help bring up M03–M05.")
story(m, "run-control", "Run control: pause, resume, step frame/scanline, reset",
    RE, "to freeze execution at an instruction boundary and resume or advance by frame/scanline",
    "I can stop the game exactly where something interesting happens",
    ["Run/Pause/Step frame/Step scanline/Reset (soft/hard) commands and hotkeys", "Focused-CPU selector", "Status bar: state, frame, line, MCLK"],
    ["Pause always stops on an instruction boundary", "Step frame advances exactly one frame", "Audio silent while paused"],
    ["Unit tests on `Machine::run` stop reasons", "Manual UI test"], specs=[S(DBG, "#run-control")])
story(m, "instruction-stepping", "Instruction stepping: into, over, out, run to cursor",
    RE, "step into/over/out and run-to-cursor on the 68k and Z80",
    "I can follow code paths precisely",
    ["Step over for JSR/BSR/TRAP and CALL/RST via stack-depth tracking", "Step out via return detection", "Run to cursor (temp breakpoint)", "Other CPU/chips advance consistently"],
    ["Step over a `jsr` lands on the following instruction even with interrupts in between", "Stepping the Z80 keeps the 68k in sync"],
    ["Scripted unit tests against synthetic ROMs"], deps=["M06-01", "M06-10"], specs=[S(DBG, "#run-control")])
story(m, "execution-breakpoints", "Execution breakpoints, conditions and logpoints",
    RE, "execution breakpoints with conditions, hit counts and logpoints",
    "I can stop only when the situation I care about occurs",
    ["Per-address bitmap per CPU", "Expression language → bytecode", "Hit counts, temp, enable/disable", "Logpoints with format strings", "Breakpoints panel"],
    ["Breakpoint on Sonic's jump routine stops when jumping", "Condition `d0.w == $10` evaluated correctly", "Performance budget (spec 01) met with 100 breakpoints active"],
    ["Unit tests for expression parser/evaluator", "Perf benchmark"], deps=["M06-01"], specs=[S(DBG, "#breakpoints")])
story(m, "watchpoints-memory-and-io", "Memory, VDP and interrupt watchpoints",
    RE, "to break on memory reads/writes, VDP register writes, DMA, and interrupts/exceptions",
    "I can find who touches a variable or hardware register",
    ["Range + size + value conditions", "VDP: register N write, DMA by destination, CRAM/VRAM/VSRAM range writes", "Interrupt level / exception vector breaks", "Break reports the accessing PC"],
    ["Write watch on Sonic's ring counter RAM stops at the ring-collect code", "DMA-to-CRAM break fires on palette loads"],
    ["Unit tests"], deps=["M06-03"], specs=[S(DBG, "#breakpoints")])
story(m, "cpu-register-views", "CPU register panels",
    RE, "68k and Z80 register panels, editable while paused",
    "I can see and change CPU state",
    ["Panels per spec 08", "Changed-since-last-stop highlighting", "SR/flags checkboxes", "Edits applied only when paused"],
    ["Editing D0 then stepping uses the new value", "Changed registers highlighted after a step"],
    ["Manual UI test"], deps=["M06-01"], specs=[S(DBG, "#inspectors-panels")])
story(m, "memory-viewers", "Memory viewers / hex editor",
    RE, "hex views of ROM, 68k RAM, Z80 RAM, VRAM, CRAM, VSRAM and SRAM",
    "I can explore and modify any memory",
    ["Virtualized hex grid with ASCII", "Go-to address/symbol, byte-pattern search", "Recent-write highlighting", "Edit while paused (ROM edits marked as patches)", "Multiple viewer instances"],
    ["Searching for a byte pattern finds all occurrences in ROM", "Edits reflected in emulation after resume"],
    ["Manual UI test; unit tests for search"], deps=["M06-01"], specs=[S(DBG, "#inspectors-panels")])
story(m, "vdp-viewers", "VDP viewers: registers, tiles, planes, sprites, palette",
    RE, "visual VDP inspectors",
    "I can see how each frame is built",
    ["Decoded registers/status", "Tile sheet with palette select", "Plane A/B/window images with viewport overlay", "Sprite list + on-screen boxes", "Palette swatches + per-line CRAM changes", "Layer toggles in the game view"],
    ["Hovering a tile shows its VRAM address and index", "Sprite list matches what's on screen in Sonic 1 GHZ"],
    ["Manual UI test"], deps=["M06-01"], specs=[S(DBG, "#inspectors-panels"), S(VDP, "#inspection-api")])
story(m, "save-states", "Save states",
    RE, "save and load full machine state to slots and named files",
    "I can return to an interesting moment instantly",
    ["Chunked, versioned format; ROM SHA-1 check", "Slots 1–9 + named states with thumbnails", "Load/save at instruction boundaries", "Hotkeys"],
    ["Determinism: save → run 600 frames → hash equals load → run 600 frames → hash", "State from another ROM refused", "Coverage unaffected by loading"],
    ["`[rom]` determinism test", "Unit tests for chunk versioning"], deps=["M06-01"], specs=[S(DBG, "#save-states")])
story(m, "trace-from-pause", "Trace from pause, ring buffer and trace on hit",
    RE, "to begin tracing from a paused state, with filters, and to dump the last N instructions after a break",
    "I can capture exactly the execution I want to study",
    ["Trace panel: configure CPU/range/first-execution-only/max size; Begin/Stop", "Text + binary output (reuses M02-06 writer)", "Always-on ring buffer (default 100k) dumpable on break", "Breakpoint action: begin trace for N frames"],
    ["Pause → begin trace → step 10 → stop yields exactly 10 records", "Ring dump after a breakpoint shows the path into it", "Tracing overhead off when not tracing"],
    ["Unit tests; manual UI flow"], deps=["M06-03"], specs=[S(DBG, "#tracing-trace-from-pause-d-007")])
story(m, "live-disassembly-view", "Live disassembly view",
    RE, "a disassembly panel that follows the PC with breakpoint gutter",
    "I always see the code that's executing",
    ["Uses M01-12 / M04-06 disassemblers", "Follow-PC, scroll back heuristically (resync on known instruction starts from coverage)", "Click gutter to toggle breakpoint", "Symbols from project DB when available"],
    ["Stepping keeps the current line highlighted", "Scrolling backwards doesn't show misaligned instructions for executed code"],
    ["Manual UI test"], deps=["M06-01"], specs=[S(DBG, "#inspectors-panels")])
story(m, "call-stack-view", "Call stack view",
    RE, "a heuristic 68k call stack",
    "I know how execution reached the current point",
    ["Shadow call stack maintained from JSR/BSR/RTS events (exact) with fallback stack-walk", "Interrupt frames marked"],
    ["Inside an object routine, stack shows main loop → object processing → routine", "V-int frames labeled"],
    ["Unit tests with synthetic ROM"], deps=["M06-02"], specs=[S(DBG, "#inspectors-panels")])

# ---------------------------------------------------------------- M07
m = milestone("M07-rom-exploration", "ROM & memory exploration",
    "Coverage recording and the color-coded ROM/RAM maps: code per CPU by execution tier (once / more than once / per frame / more than per frame), data by category (graphics, sprite, palette, level, sound, compressed, not used), watch sessions.",
    "Play Sonic 1 GHZ1: the ROM map shows 68k code in the four tiers (V-int handler = per frame, object routines = more than per frame), DAC samples and the driver as sound, art as graphics, mappings as sprite, untouched bytes as not used; arm a watch, collect a ring, stop — the watch layer highlights only the ring-collect code path; coverage persists in the project and reloads.",
    "M06, M08-01 (project DB) for persistence")
story(m, "access-instrumentation", "Coverage recorder and shadow arrays",
    DEV, "a coverage recorder attached to the hook API with per-byte shadow arrays for ROM, 68k RAM and Z80 RAM",
    "every exploration feature has accurate raw data",
    ["Flags + counters per spec 09", "Fetch vs data read; opcode vs operand bytes (needs instruction length)", "Z80 bank-window reads attributed to ROM", "Toggle on/off at runtime"],
    ["Unit tests: synthetic ROM with known code/data produces exact flags", "Performance budget met with coverage on"],
    ["Unit + perf tests"], specs=[S(COV, "#recorded-data-per-rom-byte"), S(ARCH, "#instrumentation-hook-api")])
story(m, "execution-counters-and-frame-stats", "Execution counters and per-frame statistics",
    DEV, "per-instruction counters and per-frame statistics over an observation window",
    "execution tiers can be computed cheaply and exactly",
    ["Saturating total counters, first/last frame", "`execThisFrame` (lazy reset), `maxPerFrame`", "Observation window (default 300 frames, or the active watch session): frames-executed and executions-in-window", "Dirty-list bookkeeping at frame end"],
    ["Unit tests: synthetic ROM with code run 1×, 3× total, 1×/frame and 4×/frame yields exact stats", "Bookkeeping cost stays within the spec 01 budget"],
    ["Unit + perf tests"], deps=["M07-01"], specs=[S(COV, "#recorded-data-per-rom-byte")])
story(m, "execution-tiers", "Execution tiers: once / more than once / per frame / more than per frame",
    RE, "each instruction classified per CPU as executed once, more than once, per frame, or more than once per frame",
    "I can tell init code, event code, per-frame logic and per-object/inner-loop code apart at a glance",
    ["Tier rules per spec 09 (≥ 95% of window frames; ≤ 1.5 vs > 1.5 executions per frame) — thresholds configurable", "Fallback to 'more than once' when code leaves the window", "Tier exposed to map, listing, tooltips and LLM context"],
    ["Sonic 1 boot init = once; level load = more than once; V-int handler = per frame; object routines with several objects on screen = more than per frame", "Title-screen loop is not per frame during a gameplay window"],
    ["`[rom]` scripted test asserting tiers of known routines (addresses from the reference labels)"], deps=["M07-02"], specs=[S(COV, "#execution-tiers-d-021")])
story(m, "watch-sessions", "Watch sessions and layers",
    RE, "to arm a watch, trigger an in-game event, stop, and see the code that ran because of it",
    "I can find the code behind any event",
    ["Arm/stop UI + hotkey, naming, auto-stop after N frames", "Baseline snapshot; WATCH_NEW; once-after-event", "Saved as layers; layer diff (A − B)", "Breakpoint action: arm watch"],
    ["Ring-collect watch in Sonic 1 highlights the ring touch routine and SFX trigger, not the main loop", "Two layers can be diffed"],
    ["`[rom]` scripted test: watch around a scripted ring pickup yields a stable address set"], deps=["M07-02", "M07-03"], specs=[S(COV, "#watch-sessions")])
story(m, "routine-roles-and-data-categories", "Routine roles and data categories (graphics, sprite, palette, level, sound, not used)",
    RE, "data bytes classified by what they are — graphics, sprite, palette, level, sound, compressed, other, not used",
    "I can see where every kind of asset lives in the ROM",
    ["Consume `onDma` events → category by destination", "Routine roles (decompressor+format, sprite builder, mapping/DPLC loader, palette loader, level loader, sound driver, HUD/text) tagged from UI, analysis or accepted LLM proposals",
     "Reads by role PCs set READ_BY_ROLE + category", "Z80 bank-window reads and Z80-RAM copies → sound", "Category priority per spec 09; `not-used` = untouched in all included layers and not static code"],
    ["After tagging Sonic 1's Nemesis decompressor and BuildSprites, GHZ art shows as graphics (compressed) and Sonic's mappings as sprite", "DAC samples and the Z80 driver show as sound", "Category byte totals reported in the legend"],
    ["Unit tests with a synthetic ROM", "`[rom]` spot checks against reference labels"], deps=["M07-01"], specs=[S(COV, "#routine-roles-reader-attribution"), S(COV, "#data-categories")],
    notes="Sonic games often copy palettes to a RAM buffer and DMA from RAM; add 'RAM-buffer back-tracing' as a follow-up if needed.")
story(m, "z80-code-provenance", "Z80 code provenance back to ROM",
    RE, "Z80 code executed from Z80 RAM attributed back to the ROM bytes it came from",
    "sound-driver code appears on the ROM map",
    ["Track 68k writes into Z80 RAM and match to recent ROM reads", "Confidence levels (direct copy vs decompressed block)", "Z80 RAM exec counts projected to ROM"],
    ["Sonic 1 DAC driver shows as Z80 code (high confidence) at its ROM location", "Sonic 2 compressed driver block marked Z80 (low confidence)"],
    ["`[rom]` tests"], deps=["M07-01"], specs=[S(COV, "#provenance-z80-code--rom")])
story(m, "rom-map-view", "Color-coded ROM map view",
    RE, "a zoomable, color-coded map of the whole ROM with legend and overlays",
    "I can see at a glance what every part of the ROM is",
    ["Linear (and optional Hilbert) layout, zoom 1 B/px–1 KB/px", "Incremental texture updates ≤ 4 Hz", "Colors per spec 09 for the four execution tiers and all data categories; colorblind palette", "Overlays: category, execution tier, heat, watch layer, layer diff, subsystem footprint (M11), touched-this-frame", "Legend with byte counts/percentages; per-CPU, per-tier and per-category filters"],
    ["Map updates live while playing without dropping below 60 fps", "Legend percentages sum to 100%", "Switching overlays is instant"],
    ["Manual UI test; screenshot in PR"], deps=["M07-02", "M07-03", "M07-05", "M07-06"], specs=[S(COV, "#rom-map-view")])
story(m, "rom-map-interaction", "ROM map interaction",
    RE, "hover details, click-to-navigate, range selection actions and bookmarks on the map",
    "the map is a navigation hub for analysis",
    ["Tooltip: address, symbol, categories, counts, frames, reader PC, provenance", "Click → disassembly or hex view", "Drag-select: stats, label, set type, breakpoint, queue for analysis", "Bookmarks/regions with notes"],
    ["Clicking a hot code pixel opens the disassembly at that instruction", "Range stats correct"],
    ["Manual UI test"], deps=["M07-07", "M06-06", "M06-10"], specs=[S(COV, "#interaction")])
story(m, "ram-maps", "68k RAM and Z80 RAM maps",
    RE, "read/write heat maps of 68k RAM and Z80 RAM",
    "I can discover variables, object slots and buffers",
    ["Same view component in RAM mode", "Read vs write coloring, writer PC in tooltip", "Overlay of known struct layouts (e.g. SST slots) once defined"],
    ["Sonic 1 object RAM slots visible as repeating structure", "Writer PC navigates to code"],
    ["Manual UI test"], deps=["M07-07"], specs=[S(COV, "#recorded-data-per-rom-byte")])
story(m, "coverage-persistence-and-merge", "Coverage persistence, layers and merge",
    RE, "coverage saved in the project per session, mergeable, and exportable",
    "knowledge accumulates across many play sessions",
    ["Each session stored as a layer; cumulative merged layer", "Include/exclude layers", "Export CSV of ranges + PNG of map", "`gen_cli coverage --input script` to build coverage headless"],
    ["Coverage reloads identically after restart", "Merging two sessions equals union of flags and sum of counts"],
    ["Unit tests"], deps=["M07-04", "M08-01"], specs=[S(COV, "#persistence"), S(DB)])

# ---------------------------------------------------------------- M08
m = milestone("M08-disassembly-analysis", "Disassembly & analysis",
    "The analysis toolset: project DB, code discovery seeded by coverage, jump tables, data typing, xrefs, symbols, reference-label import, listing UI, and bit-exact reassemblable export.",
    "For Sonic 1 (then 2 and 3K): exported AS source reassembles to a byte-identical ROM; code/data classification agrees with s1disasm for ≥ 99% of bytes; listing UI supports navigation, rename, comment and typing.",
    "M07 (coverage seeds), M01-12 / M04-06 (disassemblers)",
    notes="M08-01 (project DB) is needed early by M07-10; build it first.")
story(m, "project-database", "Project database",
    DEV, "the SQLite project database with migrations and text export",
    "all analysis is stored durably and diffably",
    ["Schema per spec 12", "Migrations + backup on upgrade", "Transactional writes, undo stack for renames/types", "Text export/import (`genproj`)"],
    ["Open/close/reopen preserves all data", "Text export is deterministic (same DB → same files)", "Import(export(db)) == db for exported tables"],
    ["Unit tests"], specs=[S(DB)])
story(m, "code-discovery", "Recursive-descent code discovery",
    RE, "code discovered from vectors and coverage via recursive descent with confidence levels",
    "unexecuted but reachable code is found and data isn't misread as code",
    ["Seeds: vectors, coverage exec starts, user entries", "Follow flow; stop rules; conflicts flagged", "Incremental re-runs never override user decisions", "Background job with progress"],
    ["Every executed instruction is in a code region", "No region conflicts silently resolved", "Full Sonic 1 analysis < 10 s"],
    ["Unit tests with synthetic ROMs", "`[rom]` scoring via M08-11"], deps=["M08-01", "M07-01"], specs=[S(DIS, "#code-discovery")])
story(m, "jump-table-detection", "Jump and pointer table detection",
    RE, "Sonic-style jump tables and pointer tables detected and bounded",
    "object routines and state machines are fully discovered",
    ["Idioms from spec 10", "Bounds from observed indices + heuristics", "Runtime-observed indirect targets as xrefs", "Unresolved tables flagged"],
    ["Sonic 1 object routine tables resolved with correct counts (spot-check 20 against s1disasm)"],
    ["`[rom]` tests"], deps=["M08-02"], specs=[S(DIS, "#code-discovery")])
story(m, "data-typing-and-formats", "Data typing and asset formats",
    RE, "to mark data regions with types — including compressed formats, palettes, mappings and structs — with previews",
    "data becomes meaningful and exportable as assets",
    ["Type catalog from spec 10", "Decompressors for Nemesis/Kosinski/Enigma/Kosinski-M (for previews and size detection)", "Tile/palette/mapping previews in listing", "User-defined structs"],
    ["Compressed block length auto-detected by decompressing", "Tile preview of GHZ art matches the VDP viewer"],
    ["Unit tests for each decompressor against known vectors"], deps=["M08-01"], specs=[S(DIS, "#data-typing")])
story(m, "cross-references-and-functions", "Cross-references and function boundaries",
    RE, "functions with boundaries and complete xrefs (static + observed)",
    "I can navigate callers/callees and data uses",
    ["Function = entry + reachable blocks until returns/tail jumps", "Xref kinds: call/jump/branch/read/write/pointer; observed flag", "Leaf/every-frame flags from coverage"],
    ["Xrefs to Sonic's RAM position variable list all writers", "Function boundaries agree with reference for ≥ 95% of functions"],
    ["`[rom]` scoring"], deps=["M08-02", "M08-03"], specs=[S(DIS, "#pipeline")])
story(m, "symbol-management", "Symbols, comments and structs",
    RE, "to rename, comment and define structs, with auto, hardware and RAM symbols",
    "the disassembly reads like source",
    ["Symbol sources and precedence", "Rename history", "Local labels", "Hardware register symbol set", "Struct field names in operands (`obj.x_pos(a0)` style rendering)"],
    ["Renaming updates every reference instantly", "History shows source of each rename"],
    ["Unit tests"], deps=["M08-01"], specs=[S(DIS, "#symbols")])
story(m, "reference-label-import", "Sonic Retro reference label import",
    RE, "labels from s1disasm/s2disasm/skdisasm imported as a separate, hideable symbol source",
    "I can validate my analysis and optionally work with community names",
    ["Import AS symbol/map output; record disasm commit hash", "SHA-1 alignment check", "Visible vs hidden (blind) mode", "Optional comment import"],
    ["Import for Sonic 1 succeeds when SHA-1 matches; refused otherwise", "Hidden mode removes reference names from all UI and LLM contexts"],
    ["`[rom]` test"], deps=["M08-06"], specs=[S(DIS, "#reference-label-import-d-004")])
story(m, "analysis-listing-ui", "Analysis listing UI",
    RE, "a navigable, virtualized listing of the whole ROM with keyboard-driven RE workflow",
    "I can work through the ROM efficiently",
    ["Columns: address, bytes, label, instruction/data, comment, xrefs", "Go to, back/forward, follow operand, xrefs popup, rename, comment, set type hotkeys", "Sync with ROM map and debugger", "Symbol browser panel, function list with filters (unnamed, hot, every-frame)"],
    ["Scrolling the full 4 MB S3K listing stays at 60 fps", "All hotkeys work and are documented"],
    ["Manual UI test"], deps=["M08-05", "M08-06"], specs=[S(DIS, "#listing-view-ui")])
story(m, "reassemblable-export", "Reassemblable AS export",
    RE, "the project exported as an AS source tree with build scripts",
    "I can rebuild and modify the game",
    ["Explicit sizes/branch sizes, encoding-preserving output, `dc.w` fallbacks", "Pointers as label references; relocation-risk report", "Region-grouped files, `binclude` assets", "Z80 driver as separate AS Z80 source", "Build scripts (Windows .bat/.ps1, macOS sh)"],
    ["Export completes for all three games", "Relocation-risk report lists every unresolved pointer"],
    ["Covered by M08-10"], deps=["M08-05", "M08-04", "M08-06"], specs=[S(DIS, "#reassemblable-export")])
story(m, "round-trip-verification", "Round-trip verification",
    DEV, "automated export → assemble → byte-compare",
    "the export is provably faithful",
    ["`gen_cli export --verify`", "First-difference report with address/symbol/context", "CI on homebrew ROMs; local `[rom]` for Sonic"],
    ["Sonic 1, 2 and 3K round-trip byte-identical"],
    ["`[rom]` CTest"], deps=["M08-09"], specs=[S(DIS, "#reassemblable-export")])
story(m, "accuracy-scoring", "Accuracy scoring against reference disassemblies",
    DEV, "scores comparing our code/data, function and jump-table boundaries with the reference",
    "analysis quality is measured, not guessed",
    ["Byte-level code/data agreement", "Function and jump-table agreement", "Dashboard panel and CLI report; history over time"],
    ["Report generated for each game", "Sonic 1 code/data agreement ≥ 99%"],
    ["`[rom]` CTest with thresholds"], deps=["M08-07", "M08-05"], specs=[S(DIS, "#scoring-against-the-reference")])

# ---------------------------------------------------------------- M09
m = milestone("M09-llm-decompilation", "LLM-assisted decompilation",
    "Assembly → pseudo-C, proposed names and interface contracts via the Claude API behind a pluggable provider, with context building, review workflow, batching, caching and blind evaluation — aimed at making special stages transplantable (D-019).",
    "Select any Sonic 1 function → receive pseudo-C linked line-by-line to asm and a list of name proposals → accept some → names appear everywhere. Request a contract for a special-stage function → the tool cross-checks it against xrefs and coverage and flags disagreements. Blind evaluation on 50 functions reports ≥ 60% naming agreement with reference labels.",
    "M08", notes="Sending ROM-derived disassembly to the API is allowed (D-018). Prioritize special-stage code of Sonic 1, Sonic 2 and S3K when choosing what to decompile first.")
story(m, "llm-provider-and-claude", "LLM provider interface and Claude provider",
    DEV, "an `ILlmProvider` interface with an Anthropic Claude implementation",
    "decompilation can use the best model now and other providers later",
    ["Messages API over libcurl; async with cancellation", "Structured JSON output validated against schema; one retry on failure", "Prompt-caching breakpoints", "Key from env/keychain; never logged", "Model configurable per request type; defaults to the latest Claude models", "Rate-limit/backoff handling"],
    ["Mock provider for tests", "Live smoke test (opt-in) returns schema-valid output", "Key never appears in logs or project files (test greps outputs)"],
    ["Unit tests with mock HTTP", "Opt-in live test"], specs=[S(LLM, "#provider-abstraction-d-006")])
story(m, "context-bundle-builder", "Context bundle builder",
    DEV, "a builder that assembles the per-function context from the project DB and coverage",
    "the LLM gets exactly the information a human RE would want",
    ["Sections per spec 11 (including execution tier and data categories)", "Size budget with prioritized truncation", "Blind mode substitution", "Optional UI preview of the exact payload (not a required gate, D-018)"],
    ["Bundle for a known function contains callers, callees, RAM usage and runtime facts", "Blind bundle contains no reference-sourced names"],
    ["Unit tests with fixture projects"], deps=["M09-01"], specs=[S(LLM, "#context-bundle")])
story(m, "decompile-function", "Decompile a function",
    RE, "to send a function and get pseudo-C, summary and name proposals",
    "I understand functions faster",
    ["Versioned prompt templates in repo", "Response parsing to result + proposals", "Errors surfaced with retry"],
    ["A Sonic 1 function returns schema-valid pseudo-C and ≥ 1 proposal", "Prompt version stored with result"],
    ["Mock-provider tests; opt-in live test"], deps=["M09-02"], specs=[S(LLM, "#output-schema-abridged"), S(LLM, "#workflow")])
story(m, "asm-pseudocode-view", "Side-by-side asm ↔ pseudo-C view",
    RE, "pseudo-C shown next to the asm with linked line highlighting",
    "I can verify the LLM's reading against the real code",
    ["`line_map` driven highlighting both directions", "Syntax highlighting; symbols clickable", "Version history of results per function"],
    ["Clicking a pseudo-C line highlights its asm addresses and vice-versa"],
    ["Manual UI test"], deps=["M09-03", "M08-08"], specs=[S(LLM, "#workflow")])
story(m, "naming-proposal-review", "Naming proposal review",
    RE, "to accept, edit or reject each proposed name with evidence and confidence",
    "only names I trust enter the symbol DB",
    ["Proposal list with diff, confidence, evidence", "Accept/edit/reject; bulk accept above threshold (opt-in)", "Accepted names source=`llm`, linked to result; undoable"],
    ["Accepted names show everywhere immediately", "Rejected proposals don't reappear for the same context hash"],
    ["Unit tests; manual UI"], deps=["M09-03", "M08-06"], specs=[S(LLM, "#workflow")])
story(m, "batch-and-cost-control", "Batch decompilation and cost control",
    RE, "to process a queue of functions (e.g. leaves-first every-frame functions) with cost caps",
    "large parts of the ROM get documented without babysitting",
    ["Queue builders (unnamed, hot, every-frame, bottom-up call order)", "Concurrency limit, per-run cost cap, pause/resume", "Usage and cost display"],
    ["Run stops when cost cap reached", "Results land as pending proposals"],
    ["Mock-provider tests"], deps=["M09-05"], specs=[S(LLM, "#workflow")])
story(m, "result-cache-and-provenance", "Result cache and provenance",
    DEV, "LLM results cached by content hashes and recorded with model, prompt version and usage",
    "repeat requests are free and every name is traceable",
    ["Cache key per spec 11", "Provenance fields stored", "Invalidate when bundle changes"],
    ["Identical request served from cache (no network)", "Changing a callee name changes the context hash"],
    ["Unit tests"], deps=["M09-03"], specs=[S(LLM, "#caching-and-provenance")])
story(m, "blind-evaluation", "Blind evaluation against reference labels",
    DEV, "a blind evaluation harness scoring proposed names against hidden reference labels",
    "we can measure and improve prompt/model choices",
    ["Sample selection (stratified by size/hotness)", "Exact / normalized / LLM-judged semantic match", "Report per prompt version and model"],
    ["50-function Sonic 1 report generated", "Target ≥ 60% combined agreement (success criterion 4)"],
    ["Opt-in live run; mock tests for the scoring logic"], deps=["M09-06", "M08-07"], specs=[S(LLM, "#evaluation")])
story(m, "interface-contracts", "Function interface contracts with cross-checks",
    RE, "the LLM to draft an interface contract per function (inputs, outputs, calls, hardware, assumptions) that the tools cross-check against xrefs and coverage",
    "I get trustworthy, machine-checkable descriptions of special-stage code to drive splicing",
    ["`contract` request type and schema per spec 11", "Cross-check: claimed RAM inputs/outputs vs observed reads/writes, claimed calls vs xrefs, hardware claims vs observed VDP/IO accesses", "Disagreements flagged; accept/edit per field", "Stored in `contracts` table"],
    ["A contract for a Sonic 1 special-stage routine is produced; a deliberately wrong claim (injected in a test) is flagged", "Accepted contracts are queryable by M11 tools"],
    ["Mock-provider tests with fixture coverage", "Opt-in live test"], deps=["M09-03", "M09-05"], specs=[S(LLM, "#interface-contracts-for-splicing"), S(SPLICE, "#3-contract")])

# ---------------------------------------------------------------- M10
m = milestone("M10-modding-pipeline", "Modding pipeline & first Sonic 2 mods",
    "Close the loop on the exported source: build, run, compare against the original, and validate for real hardware — proven with Sonic 2 special-stage mods (retry after failure, data-driven stage tweaks).",
    "Sonic 2 with the *retry after failure* mod is built from the app; failing a special stage offers a retry that restarts the same stage; coverage/frame diffs show no changes outside the special-stage paths; hardware-strict mode is clean; the ROM runs on a flash cart on a real NTSC console.",
    "M08 (M09 helpful for understanding S2 special-stage code)", notes="Q14 (retry behavior details) should be answered before M10-06 is Ready.")
story(m, "build-modded-rom", "Build modded ROM from the app",
    RE, "a Build ROM command that assembles an edited export, fixes the header and runs it",
    "iterating on mods is fast",
    ["AS path config + fetch script (Windows and macOS)", "Header checksum and ROM-end fix-up", "Build output/errors panel with clickable source locations", "Derived project inheriting symbols by label name", "Mods as toggleable patch sets (build-time flags)"],
    ["Edit → build → run in < 10 s for Sonic 2", "Assembler errors navigate to the source line", "Unmodified export builds a byte-identical ROM"],
    ["`[rom]` test building an unmodified export"], specs=[S(RECOMP, "#pipeline")])
story(m, "mod-diff-tools", "Mod diff tools",
    RE, "byte, symbol, coverage and frame diffs between original and mod",
    "I can confirm a mod changes only what I intended",
    ["Byte + symbol-level diff", "Coverage diff from the same scripted input", "Frame-hash diff at checkpoints", "Relocation-risk warnings before build"],
    ["A canned trivial mod shows frame diffs only after the changed behavior; coverage diff shows exactly the changed code"],
    ["`[rom]` test with a canned mod patch"], deps=["M10-01"], specs=[S(RECOMP, "#pipeline")])
story(m, "hardware-strict-mode", "Hardware-strict emulation mode",
    RE, "an emulation mode that flags behavior that works in emulators but fails on real consoles",
    "mods work on real hardware the first time",
    ["Checks from spec 14: random RAM/VRAM init (seeded), TMSS on, Z80 bus misuse, VDP bandwidth warnings, unclaimed `$A130xx` writes, header validation", "Warnings panel with PC and break option", "Strict mode selectable for scripted tests"],
    ["Unmodified Sonic 2 runs clean in strict mode (baseline)", "A test ROM violating each rule triggers the matching warning"],
    ["Unit tests with synthetic ROMs"], deps=["M10-01"], specs=[S(RECOMP, "#hardware-strict-emulation-mode")])
story(m, "real-hardware-validation", "Real-hardware validation checklist",
    RE, "a recorded checklist and report for testing each mod on real consoles via flash cart",
    "every mod is known to work outside the emulator",
    ["Checklist stored in the derived project (strict run, header, ROM size vs target cart, model 1 + model 2 NTSC flash-cart test)", "Report template for the PR"],
    ["Checklist completed for the retry mod (M10-06)"],
    ["Manual test on hardware"], deps=["M10-03"], specs=[S(RECOMP, "#hardware-validation")])
story(m, "s2-special-stage-viewers", "Sonic 2 special-stage data viewers",
    RE, "viewers for Sonic 2 special-stage data: layout segments, ring/bomb placement, checkpoint ring requirements, stage order",
    "I can understand and modify the native stages",
    ["Data types for S2 special-stage formats (spec 10 typing)", "Visual preview of segments and object placement", "Stage table view (order, requirements)"],
    ["All seven S2 stages viewable; requirements match in-game values"],
    ["`[rom]` checks against reference disassembly data labels"], deps=["M10-01"], specs=[S(SPLICE, "#6-tooling-for-s2-special-stage-modifications")])
story(m, "s2-retry-after-failure", "Sonic 2 mod: retry special stage after failure",
    PLAYER, "a \"Retry? Yes/No\" option when I fail a special stage",
    "I can try again instead of returning to the level",
    ["Hook the S2 special-stage failure result path", "Prompt UI drawn with existing S2 assets/font", "Yes: restart same stage with the same entry state (rings, stage index); No: original behavior", "Build-time options per Q14 (limit retries, cost)", "Retry path shared with spliced stages later (M11)"],
    ["Failing stage 1 offers retry; Yes restarts stage 1; No returns to the level exactly as the original", "Mod diff shows changes only in special-stage paths", "Hardware-strict mode clean; flash-cart test passed"],
    ["`[rom]` scripted test failing a stage then choosing Yes/No", "Hardware checklist (M10-04)"], deps=["M10-02", "M10-04", "M10-05"], specs=[S(RECOMP, "#special-stage-mods-for-sonic-2")])
story(m, "s2-special-stage-tweaks", "Sonic 2 mod: data-driven special-stage modifications",
    RE, "build-time options to change S2 special-stage parameters (ring requirements, stage order, selected layout/object changes)",
    "native stages can be rebalanced or remixed",
    ["Parameter table in the generated source", "Validation that layouts/objects remain within format limits", "Each tweak toggleable"],
    ["Changing checkpoint ring requirements is reflected in-game", "Default parameters build byte-identical to the original"],
    ["`[rom]` scripted tests"], deps=["M10-05"], specs=[S(RECOMP, "#special-stage-mods-for-sonic-2")])

# ---------------------------------------------------------------- M11
m = milestone("M11-special-stage-splicing", "Special-stage splicing",
    "Extract a special stage from a donor game (Sonic 1 first, then S3K Blue Sphere) with everything it depends on, and transplant it into Sonic 2.",
    "In Sonic 2, entering a special-stage ring launches the Sonic 1 special stage (per the stage table); completing it awards the correct Sonic 2 emerald, failing it returns to the level (or offers retry). The splice equivalence harness shows no unexpected divergence from the stage running in Sonic 1. Then the same for Blue Sphere.",
    "M10, M09 (contracts), M07 (watch sessions/footprints)", notes="Q12 (donors), Q13 (stage selection) and Q17 (sound) shape this milestone.")
story(m, "workspace-multi-project", "Workspace with donor and target projects",
    RE, "a workspace holding several projects (donor + target) open at once",
    "I can analyze and splice across games",
    ["`.genws` workspace file; open multiple projects", "Cross-project references by `(sha1, space, address)`", "UI: project switcher; side-by-side listings"],
    ["Sonic 1 and Sonic 2 projects open together; cross-project link navigates correctly"],
    ["Unit tests for workspace I/O"], specs=[S(DB, "#workspace-multiple-projects")])
story(m, "subsystem-definition-and-closure", "Subsystem definition and closure",
    RE, "to define a subsystem from a watch layer and root entries and get its closure of code and data, with owned/shared classification",
    "I know exactly what a special stage consists of",
    ["Seed from watch layer (enter → results)", "Roots: game-mode entry + interrupt paths", "Closure over static call graph + observed code + referenced data", "Owned vs shared classification", "Footprint overlay on the ROM map"],
    ["Sonic 1 special stage subsystem defined; spot-check of members against s1disasm labels", "Shared routines (decompressors, DMA queue, PlaySound) classified as shared"],
    ["`[rom]` test with a scripted special-stage run"], deps=["M11-01"], specs=[S(SPLICE, "#1-define-the-subsystem-donor-project")])
story(m, "resource-footprint", "Resource footprint measurement",
    RE, "the subsystem's measured RAM (scratch/inputs/outputs), VRAM/CRAM/VSRAM, VDP settings, interrupts and sound IDs",
    "I know what a stage needs from the host game",
    ["Instrumented re-run with scripted input", "RAM split into scratch/inputs/outputs", "VDP register snapshot and ranges", "Sound ID requests"],
    ["Sonic 1 special-stage footprint report generated; emerald/ring variables appear as inputs/outputs"],
    ["`[rom]` test"], deps=["M11-02"], specs=[S(SPLICE, "#2-measure-the-footprint")])
story(m, "subsystem-contract", "Subsystem contract",
    RE, "a subsystem-level contract assembled from function contracts and the footprint, with LLM-written summary and flags on disagreements",
    "the splice has a verified specification",
    ["Aggregate M09-09 contracts + footprint", "Entry/exit/assumptions sections", "Flags block splice builds until resolved"],
    ["Contract for the Sonic 1 special stage with zero open flags after review"],
    ["Fixture tests"], deps=["M11-03", "M09-09"], specs=[S(SPLICE, "#3-contract")])
story(m, "dependency-equivalence-mapping", "Shared-dependency equivalence mapping",
    RE, "each shared donor routine mapped to a target equivalent (with contract compatibility check) or marked for copying",
    "spliced code reuses target services where safe",
    ["Candidate matching (by role, name, code similarity)", "Register-contract compatibility check", "Map vs copy decision table"],
    ["S1 Nemesis decompressor maps to S2's; incompatible routines default to copy"],
    ["Unit tests"], deps=["M11-04"], specs=[S(SPLICE, "#4-splice-plan-target-sonic-2")])
story(m, "namespaced-module-export", "Namespaced, relocatable module export",
    RE, "the donor subsystem exported as a prefixed, relocatable AS module",
    "it can be dropped into the target source without collisions",
    ["Symbol prefixing (`S1SS_`)", "Only owned + copied members", "Unresolved raw references block export (relocation-risk report)"],
    ["Module assembles standalone (with stub externs)", "Zero unresolved references for Sonic 1 special stage"],
    ["Export + assemble test"], deps=["M11-05"], specs=[S(SPLICE, "#4-splice-plan-target-sonic-2")])
story(m, "ram-vram-remapping", "RAM and VRAM remapping planner",
    RE, "an automatic RAM map from donor addresses to target RAM with conflict detection against target RAM live during special stages",
    "the spliced stage can't corrupt the host game's state",
    ["Scratch → target dead-during-special-stage RAM", "Inputs/outputs → target equivalents via glue", "Conflict check using target coverage", "Editable table; VDP entry-state requirements"],
    ["Plan for S1 stage in S2 has no conflicts; deliberately overlapping mapping is flagged"],
    ["Unit tests; `[rom]` conflict check"], deps=["M11-03"], specs=[S(SPLICE, "#4-splice-plan-target-sonic-2")])
story(m, "glue-generation", "Game-mode, interrupt and result glue generation",
    DEV, "generated entry/exit shims, V-int routing and a stage selector table",
    "the spliced stage starts and finishes like a native one",
    ["Entry shim: target state → donor inputs, VDP setup", "Exit shim: donor results → S2 emerald award/rings, return game mode; calls shared retry (M10-06) and LED (M12) hooks", "V-int routine slot for the spliced stage", "Stage selector table (Q13)"],
    ["Generated glue assembles; selector routes stage N to the configured stage"],
    ["Assemble test; scripted run"], deps=["M11-06", "M11-07"], specs=[S(SPLICE, "#4-splice-plan-target-sonic-2")])
story(m, "sound-porting", "Donor sound porting",
    DEV, "donor music/SFX converted into the target's sound driver format or mapped to target IDs",
    "the spliced stage sounds right",
    ["Sound-ID translation table", "Sonic 1 68k SMPS → Sonic 2 Z80 SMPS conversion", "Fallback mapping to existing S2 tracks (Q17)"],
    ["S1 special-stage music plays in S2 (converted or mapped)", "SFX IDs translated correctly"],
    ["`[rom]` audio-hash test of the converted track"], deps=["M11-03"], specs=[S(SPLICE, "#4-splice-plan-target-sonic-2")])
story(m, "splice-equivalence-harness", "Splice equivalence harness",
    DEV, "a harness comparing the stage running in the donor vs spliced into the target, frame by frame, after applying the RAM map",
    "I can prove the transplant behaves like the original",
    ["Record owned RAM, CRAM, owned-VRAM hashes, sound IDs per frame", "Equivalent scripted input in both games", "First-divergence report with both traces; allowlist for expected differences"],
    ["Harness detects an injected divergence", "Reports are reproducible"],
    ["Unit tests; `[rom]` run"], deps=["M11-08"], specs=[S(SPLICE, "#5-validation")])
story(m, "splice-s1-into-s2", "First splice: Sonic 1 special stage into Sonic 2",
    PLAYER, "the Sonic 1 rotating special stage inside Sonic 2",
    "the splicing pipeline is proven end-to-end",
    ["Full plan → build → validate", "Retry and LED hooks invoked on exit"],
    ["Stage reachable via the selector; success awards the correct S2 emerald; failure returns (or retries)", "Equivalence harness: no unexpected divergence over the scripted run", "Strict mode clean; flash-cart test passed"],
    ["`[rom]` end-to-end scripted test", "Hardware checklist"], deps=["M11-09", "M11-10"], specs=[S(SPLICE, "#5-validation")])
story(m, "splice-blue-sphere-into-s2", "Second splice: S3K Blue Sphere into Sonic 2",
    PLAYER, "Blue Sphere stages inside Sonic 2",
    "the pipeline generalizes beyond one donor",
    ["S3K donor project analysis of Blue Sphere", "Z80 driver/sound mapping for S3K sounds", "Larger ROM placement (cart size check)"],
    ["Same acceptance as M11-11 for Blue Sphere", "Pipeline changes needed are captured as fixes to M11 tools, not one-off hacks"],
    ["`[rom]` end-to-end scripted test", "Hardware checklist"], deps=["M11-11"], specs=[S(SPLICE)])

# ---------------------------------------------------------------- M12
m = milestone("M12-cartridge-led-mod", "Sonic 2 cartridge hardware mod: emerald LEDs",
    "Emulate and support the custom Sonic 2 cartridge: the ROM writes to a `/TIME` register on each emerald collected; the cartridge latches it and lights one LED per emerald.",
    "In the emulator, the modded Sonic 2 ROM lights one virtual LED per emerald collected (native and spliced stages), state persists across restarts, and a hardware test ROM exercises the register. On the physical cartridge, the same ROM lights the same LEDs.",
    "M04-08 (cartridge device chain), M06 (watchpoints); M12-05+ need M10",
    notes="M12-01..04 can start right after M06, in parallel with M07–M10. Q15, Q16 and Q18 shape the register semantics.")
story(m, "interface-control-document", "Finalize the cartridge interface control document",
    DEV, "spec 16 finalized with the hardware builder: register addresses, semantics, signals, timing and storage",
    "the ROM, the emulator and the hardware agree exactly",
    ["Resolve Q15/Q16/Q18", "Check flash-cart register conflicts for the chosen flash cart", "Record D-022 as Accepted with final addresses"],
    ["Spec 16 reviewed and marked final; D-022 Accepted"],
    ["Review"], specs=[S(CART, "#register-interface-proposal-d-022")])
story(m, "emerald-led-device", "Emulated emerald-LED cartridge device",
    DEV, "an `EmeraldLedDevice` implementing the register interface with persistence and save-state support",
    "the mod can be developed and tested entirely in the emulator",
    ["`ICartDevice` implementation per spec 16", "Persist to `<sha1>.cartdev.json`", "Enable per SHA-1 / manual", "Save-state chunk"],
    ["Set-only semantics; clear sequence; ID/read-back behavior per spec", "State survives restart and save/load"],
    ["Unit tests"], deps=["M12-01", "M04-08"], specs=[S(CART, "#emulator-support"), S(BUS, "#cartridge-device-chain")])
story(m, "led-panel-and-log", "LED panel and device event log",
    RE, "a panel showing the seven emerald LEDs with a write log (frame, PC, value)",
    "I can see exactly when and from where the game writes",
    ["LEDs in emerald colors", "Event log; clear button; read-back toggle", "Click log entry → disassembly at PC"],
    ["Writing `$01` lights LED 1 and logs the PC"],
    ["Manual UI test"], deps=["M12-02"], specs=[S(CART, "#emulator-support")])
story(m, "prototype-ram-watch-mode", "Prototype mode: LEDs driven by RAM watch",
    RE, "virtual LEDs driven by a RAM watch on unmodified Sonic 2's emerald state",
    "LED behavior and trigger events can be validated before the ROM mod exists",
    ["Configurable RAM address/mask → LED mapping", "Uses watchpoint infrastructure"],
    ["Collecting an emerald in unmodified S2 lights the matching LED"],
    ["`[rom]` scripted test"], deps=["M12-03"], specs=[S(CART, "#emulator-support")])
story(m, "s2-rom-emerald-writes", "Sonic 2 ROM mod: write to the cartridge on emerald collected",
    RE, "the S2 results path to call `CartLed_SetEmerald` writing `1 << n` to `EMERALD_SET`",
    "the real cartridge can light its LEDs",
    ["Patch in the exported S2 source as a toggleable patch set", "Called from native results and from the splice exit glue (M11-08)", "Optional boot read-back per Q15"],
    ["Collecting emerald n writes exactly `1 << n` once", "On a cart/emulator without the device, the game behaves identically (mod diff + strict mode)"],
    ["`[rom]` scripted test with `cartdev` checkpoint", "Mod diff"], deps=["M12-02", "M10-01"], specs=[S(CART, "#game-side-behavior-rom-mod")])
story(m, "led-scripted-tests", "Scripted-test support for cartridge devices",
    DEV, "a `cartdev` checkpoint in the scripted-input format",
    "LED behavior is regression-tested",
    ["`checkpoint <frame> cartdev emerald_leds == <mask>`", "Works in strict mode"],
    ["A scripted run collecting one emerald asserts mask `$01`"],
    ["`[rom]` CTest"], deps=["M12-05", "M04-03"], specs=[S(TEST, "#scripted-input-format-tests-only"), S(CART, "#emulator-support")])
story(m, "hardware-test-rom", "Hardware bring-up test ROM",
    DEV, "a small homebrew ROM that walks the LEDs, exercises set/clear, and shows ID/read-back results on screen",
    "the physical cartridge can be brought up independently of Sonic 2",
    ["Built with AS from this repo (no commercial code)", "On-screen results; controller to step through tests", "Runs in the emulator with the device enabled"],
    ["All tests pass in the emulator", "Test ROM is redistributable (MIT)"],
    ["Emulator run in CI (homebrew ROM)"], deps=["M12-02"], specs=[S(CART, "#emulator-support")])
story(m, "physical-cart-validation", "Physical cartridge validation",
    RE, "the modded ROM and test ROM validated on the physical LED cartridge",
    "the hardware mod is done",
    ["Test ROM on the cart", "Modded S2 (with and without splices) on the cart", "Power-cycle persistence check"],
    ["Each emerald lights its LED on hardware; state persists across power cycles; stock behavior otherwise unchanged"],
    ["Manual hardware test, report in PR"], deps=["M12-05", "M12-07"], specs=[S(CART)],
    notes="Depends on the hardware build, which is outside this repository.")

# ---------------------------------------------------------------- backlog
m = milestone("backlog", "Backlog",
    "Ideas deliberately out of the current plan. Stories here are not scheduled; promote one by moving it into a milestone.",
    "—", "—", prefix="BL")
story(m, "c-recompilation-spike", "C static recompilation spike",
    DEV, "a time-boxed spike recompiling one leaf routine and one object routine of Sonic 1 to C running in lockstep with the interpreter",
    "we can decide whether C recompilation is worth pursuing",
    ["Translate to C over an emulated register file/memory API", "Lockstep oracle comparing RAM/VDP per frame", "Report: divergences, performance, effort estimate"],
    ["Both routines run in lockstep for 10 minutes of scripted play with no divergence", "Written feasibility report"],
    ["Lockstep harness"], notes="Moved to the backlog by D-017 (no widescreen; mods fit the hardware model). Time-box: 2 weeks if promoted.")
story(m, "rewind-and-input-movies", "Rewind and input movies",
    PLAYER, "rewind and recordable input movies",
    "I can step back in time and share reproducible sessions",
    ["State ring buffer for rewind", "Movie format built on the scripted-input format"],
    ["Rewind 10 s at full speed; a recorded movie replays identically"],
    ["Determinism tests"], notes="Q9 (TBD).")
story(m, "true-lock-on", "True S&K lock-on with two ROM dumps",
    PLAYER, "to load Sonic & Knuckles plus a separate locked-on ROM (S3 or S2)",
    "lock-on combinations work from original dumps",
    ["Lock-on cartridge mapping", "S&K + S2 (Knuckles in Sonic 2) support"],
    ["S&K + S3 behaves like the combined ROM"],
    ["`[rom]` boot tests"])

# ---------------------------------------------------------------- render
def story_id(m, i):
    return f"{m['prefix']}-{i:02d}"

def render_story(m, i, s):
    sid = story_id(m, i)
    lines = [f"# {sid} · {s['title']}", "",
             f"**Milestone:** [{m['prefix']} – {m['title']}](README.md)  ",
             "**Status:** Draft  ",
             f"**Depends on:** {', '.join(s['deps']) if s['deps'] else '—'}  ",
             f"**Spec refs:** {', '.join(s['specs']) if s['specs'] else '—'}", "",
             "## Story", "",
             f"As a **{s['as_a']}**, I want {s['want']}, so that {s['so_that']}.", "",
             "## Scope", ""]
    lines += [f"- {x}" for x in s["scope"]]
    lines += ["", "## Acceptance criteria", ""]
    lines += [f"- [ ] {x}" for x in s["ac"]]
    lines += ["", "## Test plan", ""]
    lines += [f"- {x}" for x in s["tests"]]
    if s["notes"]:
        lines += ["", "## Notes", "", s["notes"]]
    return "\n".join(lines) + "\n"

def render_milestone(m, files):
    lines = [f"# {m['prefix']} · {m['title']}", "",
             f"**Goal:** {m['goal']}", "",
             f"**Depends on:** {m['depends']}", "",
             "## Testable stage (exit criteria)", "", m["stage"], ""]
    if m["notes"]:
        lines += ["## Notes", "", m["notes"], ""]
    lines += ["## Stories", "", "| ID | Story | Depends on |", "|----|-------|------------|"]
    for i, (s, f) in enumerate(zip(m["stories"], files), 1):
        lines.append(f"| {story_id(m, i)} | [{s['title']}]({f}) | {', '.join(s['deps']) or '—'} |")
    return "\n".join(lines) + "\n"

total = 0
for m in M:
    d = ROOT / "stories" / m["folder"]
    d.mkdir(parents=True, exist_ok=True)
    files = []
    for i, s in enumerate(m["stories"], 1):
        f = f"{story_id(m, i)}-{s['slug']}.md"
        (d / f).write_text(render_story(m, i, s), encoding="utf-8", newline="\n")
        files.append(f)
        total += 1
    (d / "README.md").write_text(render_milestone(m, files), encoding="utf-8", newline="\n")
print(f"{len(M)} milestones, {total} stories")
