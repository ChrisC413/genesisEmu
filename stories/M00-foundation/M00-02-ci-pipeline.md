# M00-02 · CI on GitHub Actions (Windows, free tier)

**Milestone:** [M00 – Foundation](README.md)  
**Status:** Done  
**Depends on:** M00-01  
**Spec refs:** [13-testing-strategy](../../docs/specs/13-testing-strategy.md#ci)

## Story

As a **developer**, I want every push built and tested on Windows in GitHub Actions using only free-tier features, so that breakage is caught immediately without paid CI.

## Scope

- Workflow on `windows-latest` (MSVC, x64), Debug + Release
- vcpkg binary caching via `actions/cache`
- clang-format check
- Upload Windows build artifact from `main`
- No macOS runners, larger runners or paid features (D-016)
- PR template with the macOS local-validation checklist (M00-07)

## Acceptance criteria

- [x] Pushing a commit runs both Windows jobs — PR #2: `build (debug)` and `build (release)` after the `clang-format` gate
- [x] A formatting violation fails the build — run 40109907468 failed in `clang-format` and the build jobs were skipped
- [x] A failing unit test fails the build — run 40109963389: a failing CTest test (app smoke test crash) failed the `Test` step and the job
- [x] Warm-cache build completes in < 10 minutes — run 40117589455: build jobs took 2m14s (debug) and 2m26s (release), about 3 minutes end to end with the format gate
- [x] Workflow files contain no `runs-on` other than `windows-latest`

## Test plan

- Open a PR with a deliberate format error → CI fails; fix → passes
