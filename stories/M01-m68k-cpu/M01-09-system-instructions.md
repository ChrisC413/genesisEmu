# M01-09 · System and status-register instructions

**Milestone:** [M01 – Motorola 68000 CPU](README.md)  
**Status:** Draft  
**Depends on:** M01-03  
**Spec refs:** [02-cpu-m68k](../../docs/specs/02-cpu-m68k.md)

## Story

As a **developer**, I want MOVE to/from SR/CCR, MOVE USP, ANDI/ORI/EORI to CCR/SR, STOP, RESET, so that supervisor code and interrupt masking work.

## Scope

- Privilege violation on user-mode SR writes
- Stack pointer swap on S-bit change
- STOP waits for interrupt; RESET reported via hook

## Acceptance criteria

- [ ] 100% pass of corresponding SingleStepTests

## Test plan

- SingleStepTests
- Unit test: STOP resumes on an interrupt above mask
