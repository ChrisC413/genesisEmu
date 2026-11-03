# M04-03 · Scripted input for tests

**Milestone:** [M04 – Input, Z80, playable](README.md)  
**Status:** Done on Windows; macOS run pending  
**Depends on:** M04-01  
**Spec refs:** [13-testing-strategy](../../docs/specs/13-testing-strategy.md#scripted-input-format-tests-only)

## Story

As a **developer**, I want the minimal scripted-input file format in `gen_cli`, so that regression tests can play games deterministically.

## Scope

- Parser for the format in spec 13
- Checkpoints for frame and state hashes
- Not a user-facing movie feature (D-007)

## Acceptance criteria

- [ ] Script that starts Sonic 1 and walks right reaches the same frame hash every run on both OSes

## Test plan

- `[rom]` CTest

## Notes

- Format per spec 13 plus two additions: an optional expected hash on `checkpoint` lines and an optional
  `rom <sha1>` line naming the image. Parser and player in `src/common/input_script.*` (unit tested).
- `gen_cli run --input <script> [--check] [--update-script]`; `--check` without a value fails on a mismatch or
  a checkpoint without a hash. Mismatching frame checkpoints write `checkpoint_<N>.png` to `--out`.
- Scripts: tests/scripts/sonic1_ghz1.txt, sonic2_ehz1.txt, s3k_aiz1.txt; the `[rom]` test
  `Scripted input reaches the recorded checkpoints` replays them (hashes only).
- **Partly verified:** same hashes on every run on Windows (recorded in one process, re-checked by the ROM
  test in another). macOS not verified (no Mac available).
