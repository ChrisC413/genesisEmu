# M04-04 · Z80 core framework and test harnesses

**Milestone:** [M04 – Input, Z80, playable](README.md)  
**Status:** Draft  
**Depends on:** —  
**Spec refs:** [03-cpu-z80](../../docs/specs/03-cpu-z80.md#implementation-approach)

## Story

As a **developer**, I want the Z80 skeleton plus ZEXDOC/ZEXALL and SingleStepTests runners, so that Z80 instructions are verified as they're written.

## Scope

- `Z80State`, `BusZ80`, prefix decode tables, `step()` returning T-states, hooks
- CP/M BDOS shim for ZEX
- SingleStepTests JSON runner (shares code with M01-02)

## Acceptance criteria

- [ ] Runners execute and report per-opcode results

## Test plan

- Harness self-test
