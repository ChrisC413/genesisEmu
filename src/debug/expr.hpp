#pragma once
// Breakpoint condition language (spec 08 "Breakpoints", M06-03). An expression is parsed once into a small
// stack bytecode and evaluated on every hit, e.g.
//
//   d0.w == $10 && byte($FFB000) != 0
//   (a0) == 0 || word(a0+$24) & $8000      (no dereference syntax: use byte/word/long)
//
// Values are 64-bit signed integers; comparisons and logical operators give 0 or 1. Operators, from low to
// high precedence: ||, &&, |, ^, &, == !=, < <= > >=, << >>, + -, * / %, unary - ! ~.
// Literals: $1F, 0x1F, %1010, 31. Registers (68k): d0-d7, a0-a7, sp, pc, sr, ccr, usp, ssp; (Z80): a f b c
// d e h l i r af bc de hl ix iy sp pc af' bc' de' hl'. A `.b` / `.w` / `.l` suffix keeps the low 8 / 16 / 32
// bits. Functions: byte(x), word(x), long(x) read memory of the expression's CPU (big-endian on the 68k,
// little-endian words on the Z80). Variables: frame, line, value (the accessed value for a watchpoint),
// addr (the accessed address), hits (this breakpoint's hit count, including this hit).

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/hooks.hpp"

namespace gen::debug {

// What an expression reads while it is evaluated.
class EvalContext {
public:
    virtual ~EvalContext() = default;
    virtual std::int64_t reg(int id) const = 0;  // a register id from the parser (see kReg* below)
    virtual std::uint32_t read(std::uint32_t addr, int size) const = 0;  // size 1, 2 or 4
    virtual std::int64_t variable(int id) const = 0;                     // a kVar* id
};

// Register ids: 68k d0-d7 = 0-7, a0-a7 = 8-15, then the rest; Z80 ids start at kRegZ80.
enum : int {
    kRegPc = 16,
    kRegSr,
    kRegCcr,
    kRegUsp,
    kRegSsp,
    kRegZ80 = 32,  // a, f, b, c, d, e, h, l, i, r, af, bc, de, hl, ix, iy, sp, pc, af', bc', de', hl'
    kRegZ80Count = 22,
};
enum : int { kVarFrame, kVarLine, kVarValue, kVarAddr, kVarHits };

class Expr {
public:
    // Parses `text` for `cpu`'s register names. On failure returns nullopt and sets `error` (with the column).
    static std::optional<Expr> compile(std::string_view text, CpuId cpu, std::string& error);

    std::int64_t eval(const EvalContext& ctx) const;
    bool empty() const { return code_.empty(); }
    const std::string& text() const { return text_; }

private:
    friend class Parser;
    enum class Op : std::uint8_t {
        Const,
        Reg,
        Var,
        Read,  // pops an address, pushes memory of `imm` bytes
        Mask,  // keeps the low `imm` bits
        Neg,
        Not,
        BitNot,
        Mul,
        Div,
        Mod,
        Add,
        Sub,
        Shl,
        Shr,
        Lt,
        Le,
        Gt,
        Ge,
        Eq,
        Ne,
        And,
        Xor,
        Or,
        LogAnd,  // both operands are evaluated (no side effects, so short-circuit is not needed)
        LogOr,
    };
    struct Instr {
        Op op;
        std::int64_t imm;
    };
    std::vector<Instr> code_;
    int maxDepth_ = 0;
    std::string text_;
};

// A logpoint message: literal text with `{expr}` or `{expr:fmt}` fields, where fmt is `x` / `X` (hex, with
// an optional digit count such as `x4`), `d` (decimal, the default) or `c` (character). `{{` and `}}` are
// literal braces.
class Format {
public:
    static std::optional<Format> compile(std::string_view text, CpuId cpu, std::string& error);
    std::string render(const EvalContext& ctx) const;
    const std::string& text() const { return text_; }

private:
    struct Part {
        std::string literal;
        std::optional<Expr> expr;
        char style = 'd';
        int digits = 0;
    };
    std::vector<Part> parts_;
    std::string text_;
};

}  // namespace gen::debug
