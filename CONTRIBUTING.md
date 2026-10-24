# Contributing

## Prerequisites

| Platform | Needs |
|----------|-------|
| Windows 11 x64 | Visual Studio 2022 (Desktop C++ workload), CMake ≥ 3.25, vcpkg |
| macOS 13+ | Xcode Command Line Tools, CMake ≥ 3.25, vcpkg |

vcpkg runs in manifest mode ([vcpkg.json](vcpkg.json)); dependencies build on the first configure.

```sh
git clone https://github.com/microsoft/vcpkg && ./vcpkg/bootstrap-vcpkg.sh   # bootstrap-vcpkg.bat on Windows
export VCPKG_ROOT=$PWD/vcpkg                                               # $env:VCPKG_ROOT = ... in PowerShell
```

## Build and test

```sh
cmake --preset win-debug      # or win-release, mac-debug, mac-release, mac-asan
cmake --build --preset win-debug
ctest --preset win-debug
```

Outputs: `genesisEmu` (desktop app; a `.app` bundle on macOS) and `gen_cli` (headless runner). CI builds with
`-DCMAKE_COMPILE_WARNING_AS_ERROR=ON`; do the same locally before pushing.

CTest labels: `unit` (Catch2), `cli` (`gen_cli` end-to-end), `gui` (app smoke tests; reported as skipped
where no OpenGL 3.3 context is available), `rom` (needs your own ROM dumps, see below).

Tests that write user data set `GENESISEMU_HOME` so they never touch your real configuration.

## Formatting

clang-format is pinned to the version CI uses:

```sh
pip install clang-format==22.1.8
python tools/format.py          # format everything
python tools/format.py --check  # what CI runs
```

## Pull requests and CI

GitHub Actions runs on the free tier only (D-016): `windows-latest`, Debug and Release, after a
clang-format check. There are no macOS runners.

### When the macOS check is required

Run `tools/validate-mac.sh` on a Mac and paste its summary block into the PR template when a PR touches:

- platform code: `src/app/platform/`, `src/common/paths.*`, anything behind `#ifdef __APPLE__` / `_WIN32`;
- the build: `CMakeLists.txt` files, `cmake/`, `CMakePresets.json`, `vcpkg.json`;
- the audio or video backends (SDL audio, OpenGL rendering, window/Hi-DPI handling).

The script builds and tests `mac-release` (the same tests CI runs), repeats them under ASan/UBSan
(`mac-asan`), ad-hoc signs the `.app`, and exits non-zero on any failure. `--skip-asan` exists for quick
iterations but the PR summary should come from a full run.

### ROM tests

Commercial ROMs are never committed. Tests tagged `[rom]` (CTest label `rom`) read `GENESIS_ROM_DIR`
and are skipped without it:

```sh
GENESIS_ROM_DIR=~/roms tools/validate-roms.sh            # macOS
$env:GENESIS_ROM_DIR = 'D:\roms'; tools\validate-roms.ps1  # Windows
```

Unit tests must build any ROM they need in-test (see `tests/support/synthetic_rom.hpp`).

## Stories

Work follows the stories in `stories/`. Update a story's **Status** and tick only the acceptance criteria
you actually verified; note anything that could not be verified.
