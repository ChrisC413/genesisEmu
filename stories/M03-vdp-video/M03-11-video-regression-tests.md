# M03-11 · Video regression tests

**Milestone:** [M03 – VDP video](README.md)  
**Status:** Done  
**Depends on:** M03-08  
**Spec refs:** [13-testing-strategy](../../docs/specs/13-testing-strategy.md#test-data-policy)

## Story

As a **developer**, I want golden frame hashes at fixed frames of each game's boot/attract sequence, so that VDP regressions are caught.

## Scope

- `gen_cli run --check` with checkpoint frame hashes
- Golden file management per spec 13
- Diff image output on mismatch (expected/actual/diff PNG)

## Acceptance criteria

- [x] ≥ 5 checkpoints per game — `tests/golden/m03_frames.json`: Sonic 1 (6), Sonic 2 (7), S3K (6), hashes only
- [x] Mismatch produces diff images — checked with a tampered golden file: `gen_cli run --check` wrote `_actual`, `_expected` and `_diff` PNGs (the expected frame comes from a local cache of previously matching frames)

## Test plan

- `[rom]` CTest

## Notes

- `[rom]` test `tests/roms/frame_test.cpp` (CTest label `rom`, skipped without `GENESIS_ROM_DIR`) and
  `gen_cli run --rom ... --check tests/golden/m03_frames.json [--out dir]` (exit 3 on mismatch).
- Golden files hold hashes only. Frames that match are cached as raw RGBA under
  `$GENESIS_GOLDEN_CACHE` (default `<temp>/genesisEmu-golden-frames`, never in the repository), so a
  later mismatch can write expected / actual / diff PNGs; without a cached frame only the actual PNG is
  written. The test writes them to `<temp>/genesisEmu-frame-diffs`.
- Shared helpers: `src/common/golden_frames.*`, `src/common/png.*` (stored-deflate PNG writer),
  `tests/roms/rom_scan.*` (ROM discovery by SHA-1, also used by the boot test).
