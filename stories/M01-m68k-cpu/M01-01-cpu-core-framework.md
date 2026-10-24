# M01-01 · 68000 core framework: registers, bus interface, decode table

**Milestone:** [M01 – Motorola 68000 CPU](README.md)  
**Status:** Draft  
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

- [ ] Decode table construction takes < 50 ms
- [ ] Hooks fire with correct PC for every access
- [ ] An unimplemented opcode raises the illegal-instruction exception path (stubbed)

## Test plan

- Unit tests with a flat 16 MB test bus
