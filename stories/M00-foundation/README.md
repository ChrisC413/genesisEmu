# M00 · Foundation

**Goal:** Repository, build, CI and an application shell that loads a ROM — on both Windows and macOS.

**Depends on:** —

## Testable stage (exit criteria)

From a clean clone on Windows 11 and macOS 15+: build with one CMake preset, launch the app, open a ROM, see its parsed header and SHA-1. `gen_cli info --rom x` prints the same. GitHub Actions (Windows) is green and `tools/validate-mac.sh` passes locally.

## Stories

| ID | Story | Depends on |
|----|-------|------------|
| M00-01 | [Repository layout and CMake/vcpkg build](M00-01-repo-and-build-system.md) | — |
| M00-02 | [CI on GitHub Actions (Windows, free tier)](M00-02-ci-pipeline.md) | M00-01 |
| M00-03 | [Logging, configuration and platform paths](M00-03-logging-and-config.md) | M00-01 |
| M00-04 | [ROM loading and header parsing](M00-04-rom-loading-and-header.md) | M00-01 |
| M00-05 | [Application shell: SDL3 window with ImGui docking](M00-05-app-shell-window.md) | M00-01, M00-03, M00-04 |
| M00-06 | [Headless CLI runner skeleton](M00-06-headless-cli-skeleton.md) | M00-04 |
| M00-07 | [Local macOS validation script](M00-07-local-mac-validation.md) | M00-01, M00-02 |
