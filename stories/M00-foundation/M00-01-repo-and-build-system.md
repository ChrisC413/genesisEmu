# M00-01 · Repository layout and CMake/vcpkg build

**Milestone:** [M00 – Foundation](README.md)  
**Status:** Draft  
**Depends on:** —  
**Spec refs:** [01-architecture](../../docs/specs/01-architecture.md#repository-layout), [07-frontend-platform](../../docs/specs/07-frontend-platform.md#platforms)

## Story

As a **developer**, I want a CMake + vcpkg project skeleton with presets for Windows and macOS, so that everyone builds the same way from a clean clone.

## Scope

- Directory layout per architecture spec (`src/core`, `src/instrument`, `src/analysis`, `src/app`, `src/cli`, `tests`)
- `CMakePresets.json` with `win-debug/release`, `mac-debug/release`
- `vcpkg.json` manifest (SDL3, imgui[docking], sqlite3, nlohmann-json, curl, spdlog, xxhash, catch2)
- `.clang-format`, `.clang-tidy`, `.editorconfig`, `.gitignore` (ROMs, build dirs, test data)
- MIT `LICENSE` and `THIRD_PARTY_NOTICES.md` (D-014)
- Empty static libs and executables that link

## Acceptance criteria

- [ ] `cmake --preset win-debug && cmake --build --preset win-debug` succeeds on Windows 11 (MSVC 2022)
- [ ] `cmake --preset mac-debug && cmake --build --preset mac-debug` succeeds on macOS arm64
- [ ] C++20 enabled; warnings at `/W4` / `-Wall -Wextra`, treated as errors in CI
- [ ] A trivial Catch2 test runs via `ctest`

## Test plan

- Build on both platforms from a clean clone
- `ctest` passes
