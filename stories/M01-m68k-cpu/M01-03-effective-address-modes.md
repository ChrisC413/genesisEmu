# M01-03 · Effective address modes

**Milestone:** [M01 – Motorola 68000 CPU](README.md)  
**Status:** Done  
**Depends on:** M01-01, M01-02  
**Spec refs:** [02-cpu-m68k](../../docs/specs/02-cpu-m68k.md#requirements)

## Story

As a **developer**, I want all 14 68000 addressing modes with correct side effects and EA timing, so that every instruction shares one correct operand path.

## Scope

- Dn, An, (An), (An)+, -(An), d16(An), d8(An,Xn), abs.W, abs.L, d16(PC), d8(PC,Xn), #imm
- A7 byte post-inc/pre-dec by 2
- EA calculation cycle table
- Address error detection on odd word/long access (exception processing in M01-10)

## Acceptance criteria

- [x] Operand fetch/store templated on size and mode — `CpuCore::eaAddress<S, M>` / `readEa<S, M>`; handlers are instantiated per size and mode through `byMode()`
- [x] Index register sign-extension (Xn.W vs Xn.L) correct — MOVE.b/w/l, LEA, PEA, JMP, JSR pass 100% (SingleStepTests 680x0 @ e0d5ece9 (`gen_m68k_sst`, Release, 2026-10-24))
- [x] EA cycle table matches the User's Manual — verified through the suite's cycle counts for every EA combination of MOVE (0 cycle mismatches); EA time is the sum of the extension-word fetches, the operand access and 2 internal cycles for -(An) and indexed modes

## Test plan

- Pass SingleStepTests for MOVE.x (exercises all EA combinations)
