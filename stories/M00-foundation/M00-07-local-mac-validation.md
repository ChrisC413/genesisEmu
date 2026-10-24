# M00-07 · Local macOS validation script

**Milestone:** [M00 – Foundation](README.md)  
**Status:** In progress  
**Depends on:** M00-01, M00-02  
**Spec refs:** [13-testing-strategy](../../docs/specs/13-testing-strategy.md#ci), [07-frontend-platform](../../docs/specs/07-frontend-platform.md#platforms)

## Story

As a **developer**, I want a single script that builds and tests the macOS app locally and prints a PR-ready summary, so that macOS stays healthy without paying for macOS CI runners.

## Scope

- `tools/validate-mac.sh`: configure + build `mac-release`, run the same tests CI runs, ASan/UBSan build + tests, create the `.app`
- Summary block (commit, macOS version, arch, pass/fail counts, duration) for the PR
- `tools/validate-roms.(sh|ps1)` for local `[rom]` tests on either OS
- Documented in `CONTRIBUTING.md`: when the macOS check is required (platform code, CMake, audio/video backends)

## Acceptance criteria

- [ ] Script runs from a clean clone on macOS 13+ with only Xcode CLT, CMake and vcpkg installed — *unverified: no Mac available* (syntax-checked with `bash -n`)
- [ ] Non-zero exit on any failure — implemented for every step; *unverified on macOS*
- [x] Summary block pasted into the PR template matches the template's fields — same field list in `tools/validate-mac.sh` and `.github/pull_request_template.md`; the CTest-summary parser was tested against real CTest output

## Test plan

- Run on an Apple Silicon Mac; introduce a failing test and confirm a non-zero exit
