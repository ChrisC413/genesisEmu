# M02-02 · I/O, version and TMSS registers

**Milestone:** [M02 – System bus & boot](README.md)  
**Status:** Draft  
**Depends on:** M02-01  
**Spec refs:** [04-memory-bus](../../docs/specs/04-memory-bus.md#io-registers)

## Story

As a **developer**, I want version register, controller port registers (idle), and TMSS, so that boot code's region checks and port init pass.

## Scope

- Version register from config/header region
- Controller data/control registers (no input yet: idle values)
- TMSS optional (default off)

## Acceptance criteria

- [ ] Sonic 1 region check passes for its native region
- [ ] Reading port 1 returns idle (`$7F`-style) values with TH high

## Test plan

- Unit tests; boot test in M02-07
