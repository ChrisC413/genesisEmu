# M01-10 · Exceptions and interrupts

**Milestone:** [M01 – Motorola 68000 CPU](README.md)  
**Status:** Done  
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

- [x] 100% pass of SingleStepTests TRAP/CHK/TRAPV/ILLEGAL/address-error cases — TRAP, CHK, TRAPV (24,195 vectors) and every address error in the word/long files pass 100% including frames and cycles. The suite has no ILLEGAL file; illegal, line A/F and privilege violations are unit-tested (SingleStepTests 680x0 @ e0d5ece9 (`gen_m68k_sst`, Release, 2026-10-24))
- [x] Unit test: level-6 interrupt with mask 3 vectors to `$78`; with mask 6 it waits — `Autovectored interrupts respect the mask` (44 cycles), plus level 7 edge triggering
- [x] Reset reads SSP from `$0` and PC from `$4` — unit test `Reset loads SSP from $0 and PC from $4`

## Test plan

- SingleStepTests
- Interrupt unit tests

## Notes

- Address error (group 0) frame: status word = IR bits 15-5 | R/W | I/N (1 = instruction fetch) | FC, then the
  access address, IR, SR and PC; 50 cycles on top of the time spent before the faulting access.
- Trace is unit-tested (the suite never sets T). After TRAP/CHK/TRAPV/zero-divide with T set the core does not
  take a second (trace) exception; real hardware does. Not needed by the target games; noted for later.
- Interrupt timing: 44 cycles for an autovectored interrupt (spec 02); vectored and spurious (`kSpurious`)
  acknowledges are supported through `Bus68k::iack`.
