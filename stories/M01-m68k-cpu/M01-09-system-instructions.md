# M01-09 · System and status-register instructions

**Milestone:** [M01 – Motorola 68000 CPU](README.md)  
**Status:** Done  
**Depends on:** M01-03  
**Spec refs:** [02-cpu-m68k](../../docs/specs/02-cpu-m68k.md)

## Story

As a **developer**, I want MOVE to/from SR/CCR, MOVE USP, ANDI/ORI/EORI to CCR/SR, STOP, RESET, so that supervisor code and interrupt masking work.

## Scope

- Privilege violation on user-mode SR writes
- Stack pointer swap on S-bit change
- STOP waits for interrupt; RESET reported via hook

## Acceptance criteria

- [x] 100% pass of corresponding SingleStepTests — MOVEfromSR, MOVEtoSR, MOVEtoCCR, MOVEfromUSP, MOVEtoUSP, ANDI/ORI/EORI to CCR/SR, RESET: 100% of 96,780 vectors (SingleStepTests 680x0 @ e0d5ece9 (`gen_m68k_sst`, Release, 2026-10-24))

## Test plan

- SingleStepTests
- Unit test: STOP resumes on an interrupt above mask

## Notes

The suite has no STOP file and no user-mode vectors. Unit tests cover STOP resuming on an interrupt above the mask
(`STOP waits for an interrupt above the mask`), privilege violations and the USP/SSP swap (`Privileged
instructions trap in user mode and stacks switch`) and RESET reaching the bus (`RESET pulses the external reset
line`, 132 cycles).
