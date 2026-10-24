# 01 · Architecture

## Principles

1. **Headless, deterministic core.** `gen_core` has no UI, no wall-clock, no threads, no global state. Given a ROM, a starting state and an input sequence, it produces bit-identical results on every platform (D-011).
2. **Instrumentation is first-class, not bolted on.** Every memory access and instruction start can be observed through cheap hooks. Coverage (spec 09) and the debugger (spec 08) are consumers of the same hook API.
3. **Inspectable state.** Every component exposes its full state as plain structs for viewers, save states and tests.
4. **Instruction-accurate, not cycle-accurate** (D-002).

## Modules

```
gen_core        (static lib)  CPUs, bus, VDP, audio chips, scheduler, cartridge, save-state serialization
gen_instrument  (static lib)  Hook dispatch, breakpoint engine, coverage recorder, trace writer
gen_analysis    (static lib)  Disassemblers, code discovery, symbol DB, export, LLM client
gen_app         (executable)  SDL3 + Dear ImGui frontend: windows, input, audio out, all panels
gen_cli         (executable)  Headless runner: tests, batch coverage, trace dumps, export
tests/          (executables) Catch2 unit tests, CPU single-step suites, ROM regression tests
```

Dependency direction: `gen_app`/`gen_cli` → `gen_analysis` → `gen_instrument` → `gen_core`. The core never
depends on anything above it.

## Repository layout

```
CMakeLists.txt  CMakePresets.json  vcpkg.json
src/core/{m68k,z80,vdp,audio,bus,cart,sched,state}/
src/instrument/
src/analysis/{disasm68k,disasmz80,discovery,symbols,export,llm}/
src/app/{panels,platform}/
src/cli/
tests/{unit,m68k,z80,vdp,roms}/
third_party/   (only what vcpkg can't provide)
docs/  stories/
```

## Timing and scheduler

All time is counted in **master clock cycles** (MCLK, 64-bit).

| Component | Divider | NTSC rate |
|-----------|---------|-----------|
| 68000 | MCLK / 7 | 7.670 MHz |
| Z80 | MCLK / 15 | 3.580 MHz |
| YM2612 | MCLK / 7, sample every 144 cycles (6 ch × 24) | ≈ 53,267 Hz |
| PSG | MCLK / 15, divided by 16 | ≈ 223,722 Hz |
| Scanline | 3420 MCLK | 262 lines (NTSC) / 313 (PAL) |

The scheduler runs a **scanline loop**:

1. For each scanline: run the 68k instruction-by-instruction until its MCLK reaches the line's end (an instruction that crosses the boundary completes and the overshoot carries over).
2. Run the Z80 up to the same MCLK (catch-up), then audio chips up to the same MCLK.
3. Fire VDP line events: render the line, decrement the H-int counter, raise V-int at line 224 (V28) / 240 (V30), update status flags.
4. At end of frame: present framebuffer, flush audio samples, publish a `FrameEnd` hook event.

Interrupt latency and DMA stalls are modeled at instruction granularity (DMA from 68k memory freezes the 68k for an approximated number of MCLK).

The core exposes:

```cpp
RunResult Machine::run(RunLimit limit);   // limit: frames, instructions, MCLK, or "until break"
RunResult Machine::step(CpuId cpu);       // exactly one instruction on the chosen CPU
```

`RunResult` reports why it stopped (limit reached, breakpoint, watchpoint, exception, frame end).
A breakpoint stops **before** the instruction executes; the machine is always resumable from any stop.

## Threading (D-012)

Single emulation thread = the main thread. Each UI frame the app calls `run(1 frame or until break)`,
then renders panels. Audio samples are pushed into an `SDL_AudioStream`; dynamic rate control keeps the
stream fill level stable (spec 06). Heavy analysis jobs (code discovery over a whole ROM, LLM requests)
run on a worker pool and communicate through the project database; they never touch live machine state
directly — they work on snapshots.

## Instrumentation hook API

```cpp
struct Hooks {
  // Called before each instruction executes. Return Action::Break to stop.
  Action (*onInstr)(void* ctx, CpuId, uint32_t pc, uint16_t opcode);
  // Called for every CPU memory access (after address decode, before side effects).
  void   (*onAccess)(void* ctx, CpuId, AccessKind, uint32_t addr, uint8_t size, uint32_t value, uint32_t pc);
  // VDP DMA started: source, destination type (VRAM/CRAM/VSRAM), length.
  void   (*onDma)(void* ctx, const DmaEvent&);
  void   (*onIrq)(void* ctx, CpuId, int level);
  void   (*onFrameEnd)(void* ctx, uint64_t frame);
};
```

- Hooks are function pointers + context (no `std::function`, no virtual dispatch on the hot path).
- When no consumer is attached, the cost must be one predictable branch per access.
- Performance budget: full speed (60 fps) with coverage **and** an exec-breakpoint table active must use < 50% of one core on an Apple M1 / Ryzen 5 3600-class machine.

## Save-state serialization

Every component implements `serialize(StateWriter&)` / `deserialize(StateReader&)`. The format is chunked
(`tag, version, length, payload`) so new fields can be added without breaking old states. See spec 08.

## Third-party dependencies (via vcpkg, D-008)

| Library | Use |
|---------|-----|
| SDL3 | Window, input, audio, file dialogs |
| Dear ImGui (docking) + ImPlot | UI, charts in coverage views |
| SQLite3 | Project database |
| nlohmann/json | Config, LLM payloads, test vectors |
| libcurl | HTTPS for the LLM provider |
| spdlog / fmt | Logging, formatting |
| xxHash | State and frame hashes |
| Catch2 | Unit tests |
| ymfm (D-013; vendored under `third_party/` if no vcpkg port) | YM2612 synthesis |

CPU cores, VDP, PSG, bus and all analysis code are written in-house.
