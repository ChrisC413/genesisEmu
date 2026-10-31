# M01-01 · 68000 core framework: registers, bus interface, decode table

**Milestone:** [M01 – Motorola 68000 CPU](README.md)  
**Status:** Done  
**Depends on:** —  
**Spec refs:** [02-cpu-m68k](../../docs/specs/02-cpu-m68k.md#implementation-approach), [01-architecture](../../docs/specs/01-architecture.md#instrumentation-hook-api)

## Story

As a **developer**, I want the CPU skeleton — register file, bus interface, opcode decode table and `step()` loop, so that instruction groups can be added incrementally and tested in isolation.

## Scope

- `M68kState` struct (D0–D7, A0–A7, USP, SSP, PC, SR, IR, IRC, stopped)
- `Bus68k` interface (`read8/16/32`, `write8/16/32`, `iack`)
- 65,536-entry decode table built from pattern strings; unimplemented → illegal handler
- `step()` returns cycles; `onInstr` and `onAccess` hook calls (spec 01)
- Supervisor/user stack switching on SR.S change

## Acceptance criteria

- [x] Decode table construction takes < 50 ms — unit test `[m68k]` times `buildDecodeTableOnce()` (< 50 ms asserted in Release; Debug only checks < 1 s)
- [x] Hooks fire with correct PC for every access — unit test: every `onAccess` of `MOVE.L D0,(A0)+` (2 writes + prefetch) reports the instruction's PC; `onInstr` can break before an instruction
- [x] An unimplemented opcode raises the illegal-instruction exception path — fully implemented, not stubbed: vector 4 (line A: 10, line F: 11), stacked PC = the opcode, 34 cycles (unit test)

## Test plan

- Unit tests with a flat 16 MB test bus

## Notes

- `core/m68k/m68k.hpp` is the public API; `cpu_core.hpp` holds the internal machinery the handlers share.
- Timing is modeled per bus access (4 cycles per word access, added when it happens) plus explicit internal
  delays, instead of a per-instruction table. That ordering is what makes address-error frames and their cycle
  counts match the suite (address errors are 35-58% of the word/long vectors).
- Spec deviation: `Bus68k` reports wait states through a `waitCycles` counter the bus increments (cleared after
  each step) instead of a virtual call per step; read32/write32 exist but the core issues 16-bit accesses only,
  because an odd long access must fault on its first word.
