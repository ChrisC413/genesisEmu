# M10 · Modding pipeline & first Sonic 2 mods

**Goal:** Close the loop on the exported source: build, run, compare against the original, and validate for real hardware — proven with Sonic 2 special-stage mods (retry after failure, data-driven stage tweaks).

**Depends on:** M08 (M09 helpful for understanding S2 special-stage code)

## Testable stage (exit criteria)

Sonic 2 with the *retry after failure* mod is built from the app; failing a special stage offers a retry that restarts the same stage; coverage/frame diffs show no changes outside the special-stage paths; hardware-strict mode is clean; the ROM runs on a flash cart on a real NTSC console.

## Notes

Q14 (retry behavior details) should be answered before M10-06 is Ready.

## Stories

| ID | Story | Depends on |
|----|-------|------------|
| M10-01 | [Build modded ROM from the app](M10-01-build-modded-rom.md) | — |
| M10-02 | [Mod diff tools](M10-02-mod-diff-tools.md) | M10-01 |
| M10-03 | [Hardware-strict emulation mode](M10-03-hardware-strict-mode.md) | M10-01 |
| M10-04 | [Real-hardware validation checklist](M10-04-real-hardware-validation.md) | M10-03 |
| M10-05 | [Sonic 2 special-stage data viewers](M10-05-s2-special-stage-viewers.md) | M10-01 |
| M10-06 | [Sonic 2 mod: retry special stage after failure](M10-06-s2-retry-after-failure.md) | M10-02, M10-04, M10-05 |
| M10-07 | [Sonic 2 mod: data-driven special-stage modifications](M10-07-s2-special-stage-tweaks.md) | M10-05 |
