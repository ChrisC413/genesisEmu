# M02 · System bus & boot

**Goal:** Memory map, I/O, scheduler and VDP/Z80 stubs so real games boot headless; instruction tracing for bring-up.

**Depends on:** M01

## Testable stage (exit criteria)

`gen_cli run --rom <sonic> --frames 600` completes for Sonic 1, 2 and 3K with no unexpected exceptions, reaching each game's V-int wait loop; two runs produce identical state hashes; `gen_cli trace` writes an instruction trace.

## Stories

| ID | Story | Depends on |
|----|-------|------------|
| M02-01 | [68k memory map and bus](M02-01-memory-map-and-bus.md) | — |
| M02-02 | [I/O, version and TMSS registers](M02-02-io-and-version-registers.md) | M02-01 |
| M02-03 | [Z80 bus request / reset and Z80 RAM access](M02-03-z80-bus-control-stub.md) | M02-01 |
| M02-04 | [VDP port stub: status, HV counter, V-int](M02-04-vdp-port-stub.md) | M02-01, M02-05 |
| M02-05 | [Master-clock scheduler (NTSC/PAL)](M02-05-master-clock-scheduler.md) | M02-01 |
| M02-06 | [Instruction trace logger (CLI)](M02-06-instruction-trace-logger.md) | M01-12, M02-05 |
| M02-07 | [Boot smoke tests and determinism](M02-07-boot-smoke-tests.md) | M02-02, M02-03, M02-04, M02-05 |
