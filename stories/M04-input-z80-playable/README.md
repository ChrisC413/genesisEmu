# M04 · Input, Z80, playable

**Goal:** Controllers, the hand-written Z80 core and its system integration, SRAM — making all three games playable (without sound).

**Depends on:** M03

## Testable stage (exit criteria)

A human can play Sonic 1 GHZ1, Sonic 2 EHZ1 and S3K AIZ1 start to finish at full speed using keyboard or gamepad; the Z80 core passes ZEXALL and SingleStepTests; S3K saves persist across restarts.

## Stories

| ID | Story | Depends on |
|----|-------|------------|
| M04-01 | [Controller port protocol (3- and 6-button)](M04-01-controller-io.md) | — |
| M04-02 | [Keyboard/gamepad mapping and rebinding](M04-02-input-mapping.md) | M04-01 |
| M04-03 | [Scripted input for tests](M04-03-scripted-input-for-tests.md) | M04-01 |
| M04-04 | [Z80 core framework and test harnesses](M04-04-z80-core-and-harness.md) | — |
| M04-05 | [Z80 instruction set (documented + undocumented)](M04-05-z80-instruction-set.md) | M04-04 |
| M04-06 | [Z80 disassembler (AS syntax)](M04-06-z80-disassembler.md) | M04-04 |
| M04-07 | [Z80 system integration](M04-07-z80-system-integration.md) | M04-05 |
| M04-08 | [Cartridge device chain, SRAM and `$A130F1`](M04-08-sram-and-cart-devices.md) | — |
| M04-09 | [Playability milestone](M04-09-playability-milestone.md) | M04-02, M04-03, M04-07, M04-08 |
