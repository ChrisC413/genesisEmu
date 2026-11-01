# M06-00a · Play from a game snapshot

**Milestone:** [M06 – Debugger & freeze execution](README.md)  
**Status:** Done  
**Depends on:** M05 (all chips that a state must hold exist)  
**Spec refs:** [08-debugger](../../docs/specs/08-debugger.md#save-states), [01-architecture](../../docs/specs/01-architecture.md#save-state-serialization), [13-testing-strategy](../../docs/specs/13-testing-strategy.md)

## Story

As a **developer / reverse engineer**, I want to save the whole machine at any frame and start the app or a
scripted headless run straight from that snapshot, so that I can reproduce a bug or study a level without
replaying minutes of input every time.

This pulls the core of [M06-08](M06-08-save-states.md) forward; M06-08 keeps the UI around it (named states,
thumbnails, coverage interplay).

## Scope

- Full machine state, one chunk per component: 68000 (registers, prefetch, IPL, NMI edge), work RAM + TMSS
  register, VDP (registers, VRAM, CRAM, VSRAM, SAT cache, command latch, FIFO model, DMA progress, H-int
  counter, flags, and the visible framebuffer), I/O and controllers (TH protocol state), Z80 side (RAM,
  BUSREQ/RESET, bank, YM register shadow, pending 68k stall), Z80 CPU, sound (YM2612 via ymfm save/restore,
  PSG, catch-up clocks, output filters, the running audio hash), cartridge devices (SRAM contents and control
  register), and the scheduler (frame, line, MCLK positions, pending pad input, region).
- Container: `GENESTATE` magic, container version, then `tag / version / flags / length / payload` chunks
  (`core/state/save_state.hpp`). The `INFO` chunk holds the ROM SHA-1, frame, timestamp and the state hash at
  save time.
- Loading refuses: another ROM (SHA-1), another region, an unknown chunk, a chunk version newer than the build
  reads, missing chunks, corrupt or truncated data, and any state that does not reproduce its recorded
  `Machine::stateHash()` after loading. A refused load leaves the machine untouched (it is rolled back).
- App: `save-state-N` / `load-state-N` hotkeys (Shift+F1..F4 / F1..F4 by default; slots 1-9 can be bound)
  and File > Save State / Load State menus. Slots live in `<user data>/roms/<sha1>/states/slot<N>.gst`.
  `genesisEmu --state=<file> <rom>` boots straight into a snapshot. Loading flushes the audio queue and the
  resampler.
- CLI: `gen_cli run|trace --load-state <file>` and `gen_cli run --save-state-at <frame>:<file>`
  (repeatable); every option also accepts `--name=value`.
- Not in a state (by design): hooks, statistics, the bus issue log, the sound register log, mutes and mix
  settings, pending (unplayed) samples.

## Acceptance criteria

- [x] Determinism: save → run 600 frames → hash equals load → run 600 frames → hash (state and frame hash),
  in Sonic 1, Sonic 2 and S3K (`[rom]`), into a fresh machine and into the running one
- [x] State from another ROM refused, machine unchanged
- [x] Chunk versioning: newer chunk / container versions, unknown and missing chunks, wrong sizes and changed
  contents are refused with a clear message
- [x] App hotkeys and menus save / load slots; `--state=<file>` boots into a snapshot
- [x] CLI `--save-state-at` / `--load-state`: a state saved at frame 10 and run 20 frames equals 30 frames
  from power on

## Test plan

- `tests/unit/save_state_test.cpp`: archive round trips, the container, chunk versioning, wrong-ROM refusal,
  SRAM, and save/run == load/run on the synthetic boot ROM (CI).
- `tests/roms/save_state_test.cpp` `[rom][state]`: the 600-frame determinism check for every scripted game
  found in `GENESIS_ROM_DIR`, saving at the end of its input script (in a level).
- `tests/cli/CMakeLists.txt`: `cli.run_save_state` (save at 10, load and run 20 == 30 from power on),
  `cli.run_load_state_other_rom`, `cli.trace_load_state`, option errors.
- `tests/app/CMakeLists.txt`: `app.smoke_state` boots the app into the CLI-written state (`gui`).

## Notes

- Typical use: record once with the scripted input, then start every later run right before the level:

  ```sh
  gen_cli run --rom s2.bin --input go_to_cpz.txt --save-state-at=2900:cpz2.gst
  gen_cli run --rom s2.bin --load-state=cpz2.gst --frames 600 --dump-frames 3000 --out frames
  genesisEmu --state=cpz2.gst s2.bin
  ```

  State files embed RAM/VRAM contents of a commercial game: keep them out of the repository.
- The xxHash audio-hash state is written field by field (`StateHasher::serialize`); it relies on the layout of
  `XXH3_state_t` from xxHash 0.8 (pinned by vcpkg).
- Left for [M06-08](M06-08-save-states.md): named states with 160×112 thumbnails, a file dialog for arbitrary
  state files, "coverage unaffected by loading" (coverage does not exist yet), and loading older chunk versions
  (only version 1 exists).
