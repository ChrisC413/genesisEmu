# M00-02 · CI on GitHub Actions (Windows, free tier)

**Milestone:** [M00 – Foundation](README.md)  
**Status:** Draft  
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

- [ ] Pushing a commit runs both Windows jobs
- [ ] A formatting violation fails the build
- [ ] A failing unit test fails the build
- [ ] Warm-cache build completes in < 10 minutes
- [ ] Workflow files contain no `runs-on` other than `windows-latest`

## Test plan

- Open a PR with a deliberate format error → CI fails; fix → passes
