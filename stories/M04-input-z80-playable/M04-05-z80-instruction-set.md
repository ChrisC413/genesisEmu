# M04-05 · Z80 instruction set (documented + undocumented)

**Milestone:** [M04 – Input, Z80, playable](README.md)  
**Status:** Done  
**Depends on:** M04-04  
**Spec refs:** [03-cpu-z80](../../docs/specs/03-cpu-z80.md#requirements)

## Story

As a **developer**, I want every Z80 opcode including undocumented ones and X/Y flags, so that the sound drivers run exactly as on hardware.

## Scope

- Unprefixed, CB, ED, DD/FD, DDCB/FDCB
- Block instructions and repeat timing
- R, I, IFF, IM 0/1/2, HALT, EI delay, WZ

## Acceptance criteria

- [x] ZEXDOC and ZEXALL pass 100%
- [x] SingleStepTests Z80 pass 100% (or justified exclusions)

## Notes

- SingleStepTests z80 v1 (commit `ebe1875d`): **1,604,000 / 1,604,000** vectors pass, no exclusions. Every
  register incl. WZ, R, Q, the EI and LD A,I/R latches, RAM, port writes and T-states is compared. The full
  suite takes ~2 s in Release, so the Release CI job runs all of it; Debug runs 100 vectors per file.
- ZEXDOC **67/67** and ZEXALL **67/67** OK (5.76 G instructions each, ~78 s in Release at ~75 MIPS);
  they run in the nightly workflow (`.github/workflows/nightly.yml`, CTest label `z80-zex`).
- HALT leaves PC after the HALT and executes NOP M1 cycles (R advances) until an interrupt.
- Interrupts: IM 0 executes the bus byte as RST (the Genesis supplies $FF = RST 38h), IM 1, IM 2 vectored
  through I:$FF; NMI implemented; the LD A,I/R + interrupt P/V quirk is modeled.

## Test plan

- Nightly full suites
