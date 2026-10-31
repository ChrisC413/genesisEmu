# M02-02 · I/O, version and TMSS registers

**Milestone:** [M02 – System bus & boot](README.md)  
**Status:** Done  
**Depends on:** M02-01  
**Spec refs:** [04-memory-bus](../../docs/specs/04-memory-bus.md#io-registers)

## Story

As a **developer**, I want version register, controller port registers (idle), and TMSS, so that boot code's region checks and port init pass.

## Scope

- Version register from config/header region
- Controller data/control registers (no input yet: idle values)
- TMSS optional (default off)

## Acceptance criteria

- [x] Sonic 1 region check passes for its native region — `[rom]` test boots Sonic 1 REV01 600 frames with the Japan, Americas and Europe version registers (all reach the main loop, no faults)
- [x] Reading port 1 returns idle (`$7F`-style) values with TH high — `$7F` with TH high, `$33` with TH low (3-button pad ID), port 3 `$7F`

## Test plan

- Unit tests; boot test in M02-07

## Notes

- `src/core/bus/io.{hpp,cpp}`. Region: config, else header (NTSC first: Americas > Japan > Europe). Version = overseas | PAL | `$20` | (TMSS ? 1 : 0).
- The 3-button TH protocol is implemented already with an all-released button mask, so M04-01 only has to feed buttons (and add 6-button).
- TMSS (`MachineConfig::tmss`, default off): VDP accesses are dropped and logged until `'SEGA'` is written to `$A14000`.
