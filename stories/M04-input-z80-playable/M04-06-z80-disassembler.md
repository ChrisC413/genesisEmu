# M04-06 · Z80 disassembler (AS syntax)

**Milestone:** [M04 – Input, Z80, playable](README.md)  
**Status:** Done  
**Depends on:** M04-04  
**Spec refs:** [03-cpu-z80](../../docs/specs/03-cpu-z80.md#disassembler)

## Story

As a **reverse engineer**, I want a Z80 disassembler sharing the 68k disassembler's output structure, so that sound-driver code is readable in the debugger and analysis tools.

## Scope

- All documented + undocumented opcodes
- Flow info and targets

## Acceptance criteria

- [x] Round-trip through AS for all opcodes

## Test plan

- Corpus round-trip test

## Notes

- `analysis::disasmz80::disassemble()` shares the 68k disassembler's `Instruction` / `FlowType` structure
  (length, mnemonic, operands, flow, target, references, symbol resolver).
- AS syntax with `cpu z80undoc` + `relaxed on`: ixh/ixl/iyh/iyl, `sli`, `rlc (ix+$03),b`, `in f,(c)`,
  `out (c),0`. Encodings AS cannot reproduce become `db` (a prefix that changes nothing, ED mirrors, ED 63/6B,
  DDCB BIT with a register field, undefined ED opcodes).
- `gen_disasmz80_roundtrip` (CTest `analysis.disasmz80_roundtrip`): all 256 opcodes of every prefix group
  (unprefixed, CB, ED, DD, FD, DDCB, FDCB) with two operand sets, 3,584 items / 8,358 bytes, reassemble
  identically.
