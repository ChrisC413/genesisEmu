## Summary

<!-- What does this change and why? Link the story, e.g. stories/M01-m68k-cpu/M01-03-....md -->

## Checklist

- [ ] Windows CI is green (build + tests + clang-format)
- [ ] Story acceptance criteria are updated (checked only when verified)
- [ ] No ROMs, ROM-derived data or test data committed
- [ ] New third-party code is permissively licensed and listed in `THIRD_PARTY_NOTICES.md`

## macOS local validation

Required when this PR touches platform code (`src/app/platform`), CMake/presets/vcpkg, or the audio/video
backends (see `CONTRIBUTING.md`). Run `tools/validate-mac.sh` on a Mac and paste its summary block below.

- [ ] Not required for this PR
- [ ] Ran `tools/validate-mac.sh`; summary pasted below

```
genesisEmu macOS validation
commit:
macOS:
arch:
xcode clang:
release build:
release tests:
asan/ubsan build:
asan/ubsan tests:
app bundle:
duration:
result:
```

## Local ROM tests

- [ ] Ran `tools/validate-roms` with `GENESIS_ROM_DIR` set (needed when ROM loading, the core or analysis output changes)
