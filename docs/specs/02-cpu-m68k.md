# 02 · Motorola 68000 CPU

## Requirements

| ID | Requirement |
|----|-------------|
| C68-1 | Implement the complete MC68000 user + supervisor instruction set (no 68010+ instructions). |
| C68-2 | Each instruction executes atomically (`step()`), returning its exact cycle count per the 68000 User's Manual timing tables, including EA calculation and taken/not-taken branch differences. |
| C68-3 | Model all 14 addressing modes with correct register side effects (pre-decrement / post-increment, including the A7 byte-access special case of ±2). |
| C68-4 | Model exceptions: reset, bus error (not raised on Genesis but implemented), address error (odd word/long access, with correct stacked frame), illegal instruction, zero divide, CHK, TRAPV, privilege violation, trace, line-A, line-F, TRAP #0–15, spurious and auto-vectored interrupts 1–7. |
| C68-5 | Model the 2-word prefetch queue (IRC/IRD) at the register level so test-suite state matches; bus-cycle ordering is **not** modeled. |
| C68-6 | Pass the [SingleStepTests 680x0 (68000)](https://github.com/SingleStepTests/680x0) JSON suite: registers, SR, memory, and cycle counts. Known-unfixable cases (if any) must be listed explicitly with justification. |
| C68-7 | Expose the full register state (D0–D7, A0–A7, USP, SSP, PC, SR, IRC, IR, stopped/halted flags) for viewers and save states. |
| C68-8 | Call `onInstr` before each instruction and `onAccess` for every memory access, passing the instruction's PC. |
| C68-9 | Genesis quirk: `TAS` write-back is ignored on the cartridge/RAM bus (configurable, default on). |

## Implementation approach

- **Decode table**: a 65,536-entry table mapping each opcode word to a handler + pre-decoded operand info, built once at startup from pattern descriptions (e.g. `"0001 xxx xxx xxx xxx" → MOVE.B`). Illegal patterns map to the illegal-instruction handler.
- Handlers are templated on size (B/W/L) and EA mode to keep the hot path branch-light.
- The CPU talks to a `Bus68k` interface: `read8/16/32`, `write8/16/32`, `iack(level)`. The bus adds wait-state cycles when relevant (e.g. Z80-area access).
- Interrupts are sampled between instructions: a pending level > SR mask (or level 7) starts exception processing; `STOP` resumes on interrupt.
- `RESET` instruction asserts external reset (no-op on Genesis except as reported to hooks).

## Instruction groups (story breakdown)

| Group | Instructions |
|-------|--------------|
| Data movement | MOVE, MOVEA, MOVEQ, MOVEM, MOVEP, LEA, PEA, EXG, SWAP, LINK, UNLK, CLR |
| Integer arithmetic | ADD, ADDA, ADDI, ADDQ, ADDX, SUB, SUBA, SUBI, SUBQ, SUBX, CMP, CMPA, CMPI, CMPM, MULU, MULS, DIVU, DIVS, NEG, NEGX, EXT |
| Logic / shift / bit | AND, ANDI, OR, ORI, EOR, EORI, NOT, ASL/ASR, LSL/LSR, ROL/ROR, ROXL/ROXR (reg and memory), BTST, BSET, BCLR, BCHG, TST, TAS, Scc |
| BCD | ABCD, SBCD, NBCD |
| Program control | Bcc, BRA, BSR, DBcc, JMP, JSR, RTS, RTR, RTE, NOP |
| System / SR | MOVE to/from SR, MOVE to CCR, MOVE USP, ANDI/ORI/EORI to CCR/SR, STOP, RESET, TRAP, TRAPV, CHK, ILLEGAL |

## Cycle timing notes

- MULU/MULS and DIVU/DIVS timings are data dependent; implement the exact formulas (MULU: 38 + 2n where n = number of 1 bits in the source; MULS: 38 + 2n where n counts 01/10 transitions; DIV per the documented algorithm, as validated by the test suite).
- Shifts: 6/8 + 2n.
- Bcc: taken 10, not taken 8 (.B) / 12 (.W). DBcc: per manual.
- Exceptions: group-specific (interrupt = 44 cycles incl. IACK on Genesis autovector).

## Disassembler (lives in `gen_analysis` but delivered in M01)

- Motorola syntax compatible with **AS** (D-010): `move.w  (a0)+,d1`, `$` hex, explicit size suffixes always emitted.
- Input: bytes + address. Output: mnemonic, operands, length, branch/jump targets, flow type (fallthrough / branch / call / return / jump-indirect / trap).
- Must decode **every** opcode the CPU core decodes, and mark the rest `dc.w $XXXX` (illegal).
- Verified by round-trip: disassemble → assemble with AS → identical bytes, over all 65,536 first words with representative extension words.

## Implementation notes (M01)

- Timing is accumulated per bus access (4 cycles per word, when it happens) plus explicit internal delays rather
  than looked up per instruction. The order of accesses inside each instruction follows what the SingleStepTests
  address-error frames reveal, so faults stack the right PC and cost the right time.
- `Bus68k` reports wait states through a `waitCycles` counter (cleared after each step) instead of a virtual call.
- SingleStepTests exclusions (C68-6) are listed with reasons in `tests/m68k/sst_exclusions.txt` and summarized in
  story M01-11: 3,736 ASR vectors where the suite contradicts the PRM's carry rule, and 2 corrupt ASL.b vectors.
- The disassembler prints PC-relative operands and branches as target addresses (AS computes the displacement)
  and emits `dc.w` for encodings AS cannot reproduce byte for byte; see `analysis/disasm68k/disasm68k.hpp`.
