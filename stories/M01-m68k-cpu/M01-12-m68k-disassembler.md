# M01-12 · 68000 disassembler (AS syntax)

**Milestone:** [M01 – Motorola 68000 CPU](README.md)  
**Status:** Draft  
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

- [ ] Every opcode the core decodes is disassembled
- [ ] Round-trip test: generated instruction corpus → disassemble → assemble with AS → identical bytes
- [ ] Explicit sizes always emitted

## Test plan

- Round-trip corpus test (AS fetched by the test-data script)
- Golden text tests for tricky encodings (MOVEM lists, PC-relative, BCD)

## Notes

Can be developed in parallel with the instruction groups.
