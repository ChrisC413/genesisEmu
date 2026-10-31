#pragma once
// Z80 disassembler producing Zilog syntax that The Macro Assembler AS accepts with `cpu z80undoc` and
// `relaxed on` (spec 03 "Disassembler", story M04-06). Same output structure as the 68000 disassembler.
//
// Conventions: lowercase mnemonics, `$` hex (`ld (ix+$05),a`, `ld a,($1234)`), relative jumps print the
// target address, undocumented forms use AS's names (ixh/ixl/iyh/iyl, sli, `rlc (ix+$03),b`, `in f,(c)`,
// `out (c),0`). Encodings AS cannot reproduce byte for byte (a DD/FD prefix that changes nothing, the ED
// mirrors of NEG / RETN / IM, ED 63/6B, DDCB BIT with a register field, undefined ED opcodes) become `db`
// with flow Invalid.

#include <cstdint>
#include <span>

#include "analysis/disasm68k/disasm68k.hpp"

namespace gen::analysis::disasmz80 {

using FlowType = disasm68k::FlowType;
using Instruction = disasm68k::Instruction;
using SymbolResolver = disasm68k::SymbolResolver;

// Disassembles one instruction at `address` from `bytes` (which start at that address). Missing operand
// bytes make the instruction invalid (db of the first byte). Conditional RET is ConditionalBranch without a
// target; RST is a Call to its vector; HALT is Stop.
Instruction disassemble(std::span<const std::uint8_t> bytes, std::uint16_t address,
                        const SymbolResolver& symbols = {});

}  // namespace gen::analysis::disasmz80
