# 13 · Testing Strategy

## Layers

| Layer | What | Where it runs |
|-------|------|---------------|
| Unit | Bus decoding, VDP command parsing, PSG, config, symbol DB, disassembler cases | CI |
| CPU single-step | SingleStepTests 68000 (~1M cases) and Z80 JSON suites | CI (subset per push, full nightly) |
| CPU programs | ZEXDOC / ZEXALL for Z80 | CI nightly |
| Hardware test ROMs | Freely redistributable homebrew test ROMs (VDP, DMA, controller, audio) with golden screenshots | CI |
| Determinism | Run ROM N frames twice (and save/load mid-way) → identical state hashes | CI with homebrew ROMs; local with Sonic |
| Game regression | Sonic 1/2/3K with scripted input → golden frame hashes at checkpoints, audio hash | Local (`GENESIS_ROM_DIR`) |
| Analysis | Disassembler round-trip (AS), export round-trip on homebrew ROMs; on Sonic ROMs locally | CI / local |
| Performance | Frames/sec headless benchmark per game with coverage on/off | Local + nightly trend |

## Test data policy

- Test suites with permissive licenses are fetched by a script (`tools/fetch-test-data`) into an ignored folder, pinned by commit hash and checksum — not vendored.
- Commercial ROMs are never committed. Tests that need them are tagged `[rom]` and skipped when `GENESIS_ROM_DIR` is unset or the SHA-1 isn't found.
- Golden hashes are stored in `tests/golden/*.json` with the emulator version that produced them; updating goldens requires a commit message tag `[golden-update]` and a reason.

## Scripted input format (tests only)

A minimal, line-oriented text file:

```
# frame  port  buttons
0        1     -
120      1     START
121      1     -
300      1     RIGHT
420      1     RIGHT+C
checkpoint 600 frame_hash   # assert the frame hash at frame 600
checkpoint 600 state_hash
```

`gen_cli --rom sonic1.bin --input script.txt --check` runs headless and exits non-zero on mismatch.

## CI

D-016: GitHub Actions on the **free tier only** — standard `windows-latest` runners, the standard
Actions cache, and default artifact retention. No larger runners, no paid minutes, no macOS runners.

- **GitHub Actions** (Windows 11-compatible `windows-latest`, MSVC, x64): Debug + Release.
  Steps: configure (vcpkg binary cache via `actions/cache`), build, unit tests, CPU subset, test-ROM tests, clang-format check, clang-tidy (warnings as errors on new code).
- **Nightly (scheduled workflow, Windows)**: full CPU suites, ZEXALL. Must stay within the free-tier minute budget; if it doesn't, run nightly only on the default branch and trim the suite.
- **macOS: local validation.** `tools/validate-mac.sh` configures, builds (`mac-release`), runs the same test set CI runs plus an ASan/UBSan build, and creates the `.app`. It prints a summary block to paste into the PR (commit hash, macOS version, pass/fail counts). PRs that touch platform code (`src/app/platform`, CMake, audio/video backends) need this summary before merge — enforced by the PR template checklist, not by CI.
- **Local ROM tests**: `tools/validate-roms.(sh|ps1)` runs all `[rom]` tests against `GENESIS_ROM_DIR` on either OS.
- Artifacts: Windows zip from `main`. macOS `.app` builds are produced locally.

## Reference comparison (debug aid)

A `trace-diff` tool compares our text trace with a trace exported from a reference emulator (converted to
our format) and reports the first divergence with surrounding context — the main tool for M02/M03 bring-up.
