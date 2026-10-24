# M04-05 · Z80 instruction set (documented + undocumented)

**Milestone:** [M04 – Input, Z80, playable](README.md)  
**Status:** Draft  
**Depends on:** M04-04  
**Spec refs:** [03-cpu-z80](../../docs/specs/03-cpu-z80.md#requirements)

## Story

As a **developer**, I want every Z80 opcode including undocumented ones and X/Y flags, so that the sound drivers run exactly as on hardware.

## Scope

- Unprefixed, CB, ED, DD/FD, DDCB/FDCB
- Block instructions and repeat timing
- R, I, IFF, IM 0/1/2, HALT, EI delay, WZ

## Acceptance criteria

- [ ] ZEXDOC and ZEXALL pass 100%
- [ ] SingleStepTests Z80 pass 100% (or justified exclusions)

## Test plan

- Nightly full suites
