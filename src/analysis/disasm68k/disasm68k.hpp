#pragma once
// 68000 disassembler producing AS-compatible Motorola syntax (spec 02, spec 10, story M01-12).
//
// Output conventions (all accepted by The Macro Assembler AS and reassembled to identical bytes):
//   - lowercase mnemonics with an explicit size suffix wherever AS accepts one (move.w, lea.l, bra.s, dbf.w);
//   - `$` hex; absolute operands as ($1234).w / ($00FF0000).l, abs.W printed sign-extended ($FFFF8000).w;
//   - PC-relative operands and branches print the *target address* (AS computes the displacement);
//   - A7 prints as sp.
// Encodings AS cannot reproduce byte for byte (68020 extension-word bits a 68000 ignores, a byte immediate
// with a non-zero high byte, an empty MOVEM list, ...) and every opcode the 68000 does not decode become
// `dc.w $XXXX` with flow Invalid.

#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace gen::analysis::disasm68k {

enum class FlowType : std::uint8_t {
    Fallthrough,        // continues with the next instruction
    Branch,             // unconditional direct transfer (BRA, JMP abs/PC-relative)
    ConditionalBranch,  // Bcc, DBcc: target or fallthrough
    Call,               // BSR, JSR with a known target
    Return,             // RTS, RTR, RTE
    JumpIndirect,       // JMP through a register
    CallIndirect,       // JSR through a register
    Trap,               // TRAP, ILLEGAL, line A/F: enters an exception handler
    Stop,               // STOP: waits for an interrupt, then falls through
    Invalid,            // not a 68000 instruction (emitted as dc.w)
};

struct Instruction {
    std::uint32_t address = 0;
    std::uint8_t length = 2;  // bytes
    std::string mnemonic;     // "move.w", "dc.w", ...
    std::string operands;     // "(a0)+,d1"
    FlowType flow = FlowType::Invalid;
    std::optional<std::uint32_t> target;    // branch/jump/call destination when known statically
    std::vector<std::uint32_t> references;  // absolute addresses referenced (abs.W/L, PC-relative, targets)

    // "mnemonic operands" with a tab between them (or just the mnemonic).
    std::string text() const;
    bool valid() const { return flow != FlowType::Invalid; }
};

// Maps an address to a symbol name used in place of the number (nullopt keeps the number).
using SymbolResolver = std::function<std::optional<std::string>(std::uint32_t address)>;

// Disassembles one instruction at `address` from `bytes` (which start at that address). Missing extension
// words are treated as invalid (dc.w of the first word).
Instruction disassemble(std::span<const std::uint8_t> bytes, std::uint32_t address,
                        const SymbolResolver& symbols = {});

// True if the first word is a valid 68000 opcode by itself (independent of extension words).
bool isValidFirstWord(std::uint16_t opcode);

}  // namespace gen::analysis::disasm68k
