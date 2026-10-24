# M02-07 · Boot smoke tests and determinism

**Milestone:** [M02 – System bus & boot](README.md)  
**Status:** In progress  
**Depends on:** M02-02, M02-03, M02-04, M02-05  
**Spec refs:** [13-testing-strategy](../../docs/specs/13-testing-strategy.md#layers)

## Story

As a **developer**, I want automated boot tests for all three games plus a determinism check, so that regressions in CPU/bus/scheduler are caught immediately.

## Scope

- `[rom]` tests: run 600 frames per game; assert no illegal/address-error exceptions; assert PC is in the V-int wait loop at frame end
- State hash (xxHash of all component state)
- Run twice → same hash

## Acceptance criteria

- [x] All three games pass locally with `GENESIS_ROM_DIR` set — Windows 11, MSVC Release, 2026-10-24: Sonic 1 REV01, Sonic 2 REV01 and S3K (S&K + Sonic 3 USA, built with `lockOnImage`) each run 600 frames with no fault exceptions, end in their V-int wait loop and give the same state hash twice and as recorded in `tests/golden/m02_boot.json`
- [x] Tests skip cleanly without ROMs — `gen_rom_tests` exits 4 (CTest "skipped") without `GENESIS_ROM_DIR`
- [ ] State hash is identical on Windows and macOS — *not verified*: no Mac was available. The hash is XXH3-64 over explicitly little-endian serialized fields (no struct memory), and the goldens hold the Windows hashes, so `tools/validate-roms.sh` on a Mac checks this

## Test plan

- CTest `[rom]` label

## Notes

- `tests/roms/boot_test.cpp`. ROMs are found by SHA-1; the golden file stores only SHA-1s, wait-loop addresses and state hashes.
- Wait-loop check (generic, no per-game code): over the last 60 of 600 frames, at least 90% of frames take exactly one V-int; at least 90% of the V-ints interrupt a window of fewer than 16 bytes; and during 5 more frames, an instruction in that window reads a work-RAM byte that the V-int handler (interrupt mask 6) writes. The window must lie inside the golden range.
- Results (i7-11700KF, Release): Sonic 1 loop $29A4-$29A8, 0.18 ms/frame; Sonic 2 loop $3388-$338C (one lag frame in 60), 0.18 ms/frame; S3K loop $1D1C-$1D20, 0.16 ms/frame.
- None of the games waits on the Z80 within 600 frames; S3K writes to its SRAM range ($200001+), which is logged as ROM writes until M04-08.
- Determinism in CI uses the synthetic boot ROM (`[determinism]` unit test and `cli.run_boot_json`, which compares the state hash of two runs).
