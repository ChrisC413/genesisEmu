# M04-04 · Z80 core framework and test harnesses

**Milestone:** [M04 – Input, Z80, playable](README.md)  
**Status:** Done  
**Depends on:** —  
**Spec refs:** [03-cpu-z80](../../docs/specs/03-cpu-z80.md#implementation-approach)

## Story

As a **developer**, I want the Z80 skeleton plus ZEXDOC/ZEXALL and SingleStepTests runners, so that Z80 instructions are verified as they're written.

## Scope

- `Z80State`, `BusZ80`, prefix decode tables, `step()` returning T-states, hooks
- CP/M BDOS shim for ZEX
- SingleStepTests JSON runner (shares code with M01-02)

## Acceptance criteria

- [x] Runners execute and report per-opcode results

## Notes

- `gen_z80_sst` (tests/z80/ssz_runner.cpp) streams the 1,604 SingleStepTests z80 files converted by
  `tools/fetch-test-data.py --only z80` and prints a line per opcode file with failures plus a total. The
  self-test (`--inject-fault --expect-fail`) proves a broken core is reported.
- `gen_z80_zex` (tests/z80/zex_runner.cpp) runs ZEXDOC/ZEXALL through a BDOS shim (C=2, C=9, warm boot).
- The 68k runner is not shared: the formats differ enough that a common reader would only share the file
  header check.
- Hooks: `onInstr` / `onAccess` / `onIrq` with `CpuId::Z80` and Z80-local addresses.

## Test plan

- Harness self-test
