# M07 · ROM & memory exploration

**Goal:** Coverage recording and the color-coded ROM/RAM maps: code per CPU by execution tier (once / more than once / per frame / more than per frame), data by category (graphics, sprite, palette, level, sound, compressed, not used), watch sessions.

**Depends on:** M06, M08-01 (project DB) for persistence

## Testable stage (exit criteria)

Play Sonic 1 GHZ1: the ROM map shows 68k code in the four tiers (V-int handler = per frame, object routines = more than per frame), DAC samples and the driver as sound, art as graphics, mappings as sprite, untouched bytes as not used; arm a watch, collect a ring, stop — the watch layer highlights only the ring-collect code path; coverage persists in the project and reloads.

## Stories

| ID | Story | Depends on |
|----|-------|------------|
| M07-01 | [Coverage recorder and shadow arrays](M07-01-access-instrumentation.md) | — |
| M07-02 | [Execution counters and per-frame statistics](M07-02-execution-counters-and-frame-stats.md) | M07-01 |
| M07-03 | [Execution tiers: once / more than once / per frame / more than per frame](M07-03-execution-tiers.md) | M07-02 |
| M07-04 | [Watch sessions and layers](M07-04-watch-sessions.md) | M07-02, M07-03 |
| M07-05 | [Routine roles and data categories (graphics, sprite, palette, level, sound, not used)](M07-05-routine-roles-and-data-categories.md) | M07-01 |
| M07-06 | [Z80 code provenance back to ROM](M07-06-z80-code-provenance.md) | M07-01 |
| M07-07 | [Color-coded ROM map view](M07-07-rom-map-view.md) | M07-02, M07-03, M07-05, M07-06 |
| M07-08 | [ROM map interaction](M07-08-rom-map-interaction.md) | M07-07, M06-06, M06-10 |
| M07-09 | [68k RAM and Z80 RAM maps](M07-09-ram-maps.md) | M07-07 |
| M07-10 | [Coverage persistence, layers and merge](M07-10-coverage-persistence-and-merge.md) | M07-04, M08-01 |
