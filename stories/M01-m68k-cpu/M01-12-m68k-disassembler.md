# M01-12 · 68000 disassembler (AS syntax)

**Milestone:** [M01 – Motorola 68000 CPU](README.md)  
**Status:** Done  
**Depends on:** M01-01  
**Spec refs:** [02-cpu-m68k](../../docs/specs/02-cpu-m68k.md#disassembler-lives-in-gen_analysis-but-delivered-in-m01), [10-disassembler-and-analysis](../../docs/specs/10-disassembler-and-analysis.md)

## Story

As a **reverse engineer**, I want a 68000 disassembler producing AS-compatible Motorola syntax with flow information, so that the debugger, tracer and analysis tools all share one correct disassembly.

## Scope

- Library in `gen_analysis/disasm68k`
- Output: text, length, flow type, branch/jump targets, referenced absolute addresses
- Symbol resolver callback for operand naming
- Illegal patterns → `dc.w $XXXX`

## Acceptance criteria

- [x] Every opcode the core decodes is disassembled — unit test: `isValidFirstWord()` equals the core's decode table for all 65,536 words (45,815 valid + ILLEGAL); the round trip confirms each is emitted as an instruction with at least one extension-word set
- [x] Round-trip test: generated instruction corpus → disassemble → assemble with AS → identical bytes — `gen_disasm_roundtrip` (CTest `analysis.disasm_roundtrip`): 196,608 instructions (every first word × 3 extension-word sets; 136,765 decoded, 59,843 dc.w), 505,282 bytes, 0 mismatches with AS 1.42 build 315 (fetched and SHA-256-pinned by `tools/fetch-test-data.py --only as`; Windows binary only, other platforms set `GENESIS_ASL`)
- [x] Explicit sizes always emitted — wherever AS accepts a size suffix (`lea.l`, `moveq.l`, `bra.s`, `dbf.w`, `btst.l`/`.b`, ...). AS rejects one on `swap`, `jmp`, `jsr`, `unlk`, `trap`, `stop` and the no-operand instructions, so those are emitted without

## Test plan

- Round-trip corpus test (AS fetched by the test-data script)
- Golden text tests for tricky encodings (MOVEM lists, PC-relative, BCD)

## Notes

Can be developed in parallel with the instruction groups.


- `gen_analysis` → `analysis/disasm68k/disasm68k.hpp`. Decoding is written separately from the core's decode
  table; the unit test keeps the two in agreement.
- Output: text, length, flow type (fallthrough, branch, conditional branch, call, return, indirect jump/call,
  trap, stop, invalid), static target, referenced absolute addresses; optional symbol resolver.
- PC-relative operands and branch targets print the target address (`$00001004(pc)`), since AS computes the
  displacement. abs.W prints sign-extended (`($FFFF8000).w`).
- Encodings AS cannot reproduce (68020 index-word bits, byte immediates or CCR immediates with a non-zero high
  byte, static bit numbers with a non-zero high byte, empty MOVEM lists) are emitted as `dc.w`.
