# M01-10 · Exceptions and interrupts

**Milestone:** [M01 – Motorola 68000 CPU](README.md)  
**Status:** Draft  
**Depends on:** M01-04, M01-05, M01-09  
**Spec refs:** [02-cpu-m68k](../../docs/specs/02-cpu-m68k.md#requirements)

## Story

As a **developer**, I want the full 68000 exception model: reset, address error, illegal, line-A/F, privilege, trace, TRAP, TRAPV, CHK, zero divide, autovectored interrupts, so that the CPU can run real software and the debugger can break on exceptions.

## Scope

- Exception stack frames (group 0 long frame for address error)
- Interrupt sampling between instructions, level 7 non-maskable, IPL mask update
- Autovector IACK cycles; spurious interrupt
- Trace exception after each instruction when T=1
- `onIrq` hook

## Acceptance criteria

- [ ] 100% pass of SingleStepTests TRAP/CHK/TRAPV/ILLEGAL/address-error cases
- [ ] Unit test: level-6 interrupt with mask 3 vectors to `$78`; with mask 6 it waits
- [ ] Reset reads SSP from `$0` and PC from `$4`

## Test plan

- SingleStepTests
- Interrupt unit tests
