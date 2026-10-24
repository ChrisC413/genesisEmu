# M01 · Motorola 68000 CPU

**Goal:** A hand-written, instruction-by-instruction 68000 core with exact cycle counts, plus a 68k disassembler.

**Depends on:** M00

## Testable stage (exit criteria)

Headless test binary runs the full SingleStepTests 68000 suite: 100% pass (or an explicit, justified exclusion list), including cycle counts. Disassembler round-trips through AS for every valid opcode pattern.

## Stories

| ID | Story | Depends on |
|----|-------|------------|
| M01-01 | [68000 core framework: registers, bus interface, decode table](M01-01-cpu-core-framework.md) | — |
| M01-02 | [SingleStepTests 68000 harness](M01-02-singlesteptests-harness.md) | M01-01 |
| M01-03 | [Effective address modes](M01-03-effective-address-modes.md) | M01-01, M01-02 |
| M01-04 | [Data movement instructions](M01-04-data-movement.md) | M01-03 |
| M01-05 | [Integer arithmetic instructions](M01-05-integer-arithmetic.md) | M01-03 |
| M01-06 | [Logic, shift/rotate and bit instructions](M01-06-logic-shift-bit.md) | M01-03 |
| M01-07 | [BCD instructions](M01-07-bcd-instructions.md) | M01-03 |
| M01-08 | [Program control instructions](M01-08-program-control.md) | M01-03 |
| M01-09 | [System and status-register instructions](M01-09-system-instructions.md) | M01-03 |
| M01-10 | [Exceptions and interrupts](M01-10-exceptions-and-interrupts.md) | M01-04, M01-05, M01-09 |
| M01-11 | [Cycle timing audit](M01-11-cycle-timing-audit.md) | M01-04, M01-05, M01-06, M01-07, M01-08, M01-09, M01-10 |
| M01-12 | [68000 disassembler (AS syntax)](M01-12-m68k-disassembler.md) | M01-01 |
