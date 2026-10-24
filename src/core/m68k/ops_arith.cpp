// Integer arithmetic: ADD/SUB/CMP families, ADDX/SUBX, MULU/MULS, DIVU/DIVS, NEG/NEGX, EXT (story M01-05).

#include <bit>

#include "core/m68k/cpu_core.hpp"

namespace gen::m68k {

namespace {

constexpr int kVectorZeroDivide = 5;

enum class Op { Add, Sub, Cmp };

// Adds or subtracts with full flag handling. `extend` selects ADDX/SUBX/NEGX semantics: X is an input and Z
// is only ever cleared.
template <int S, Op O>
std::uint32_t arith(CpuCore& c, std::uint32_t src, std::uint32_t dst, bool extend = false) {
    constexpr std::uint32_t mask = sizeMask<S>();
    constexpr std::uint32_t msb = signBit<S>();
    src &= mask;
    dst &= mask;
    const std::uint64_t x = extend && c.flag(kSrExtend) ? 1 : 0;
    std::uint64_t wide = 0;
    std::uint32_t r = 0;
    bool overflow = false;
    if constexpr (O == Op::Add) {
        wide = static_cast<std::uint64_t>(dst) + src + x;
        r = static_cast<std::uint32_t>(wide) & mask;
        overflow = (((src ^ r) & (dst ^ r)) & msb) != 0;
    } else {
        wide = static_cast<std::uint64_t>(dst) - src - x;
        r = static_cast<std::uint32_t>(wide) & mask;
        overflow = (((src ^ dst) & (r ^ dst)) & msb) != 0;
    }
    const bool carry = ((wide >> (8 * S)) & 1) != 0;
    std::uint16_t sr = c.s.sr;
    sr &= static_cast<std::uint16_t>(~(kSrCarry | kSrOverflow | kSrNegative));
    if constexpr (O != Op::Cmp) {
        sr = static_cast<std::uint16_t>((sr & ~kSrExtend) | (carry ? kSrExtend : 0));
    }
    sr |= static_cast<std::uint16_t>((carry ? kSrCarry : 0) | (overflow ? kSrOverflow : 0) |
                                     ((r & msb) != 0 ? kSrNegative : 0));
    if (extend) {
        if (r != 0) {
            sr &= static_cast<std::uint16_t>(~kSrZero);
        }
    } else {
        sr = static_cast<std::uint16_t>((sr & ~kSrZero) | (r == 0 ? kSrZero : 0));
    }
    c.s.sr = sr;
    return r;
}

// <ea>,Dn  (ADD, SUB, CMP)
template <int S, Op O, Mode M>
void opArithToReg(CpuCore& c, std::uint32_t op) {
    const std::uint32_t src = c.readEa<S, M>(op & 7);
    const unsigned dn = (op >> 9) & 7;
    const std::uint32_t r = arith<S, O>(c, src, c.s.d[dn]);
    if constexpr (O != Op::Cmp) {
        c.writeDreg<S>(dn, r);
    }
    c.prefetch();
    if constexpr (S == 4) {
        if constexpr (O == Op::Cmp) {
            c.idle(2);
        } else {
            c.idle(M == DReg || M == AReg || M == Imm ? 4 : 2);
        }
    }
}

// Dn,<ea>  (ADD, SUB to memory)
template <int S, Op O, Mode M>
void opArithToMem(CpuCore& c, std::uint32_t op) {
    const std::uint32_t addr = c.eaAddress<S, M>(op & 7);
    const std::uint32_t dst = c.read<S>(addr);
    const std::uint32_t r = arith<S, O>(c, c.s.d[(op >> 9) & 7], dst);
    c.prefetch();
    c.write<S>(addr, r);
}

// ADDA / SUBA / CMPA: whole-register operation on the sign-extended source.
template <int S, Op O, Mode M>
void opArithAddr(CpuCore& c, std::uint32_t op) {
    const auto src = static_cast<std::uint32_t>(signExtend<S>(c.readEa<S, M>(op & 7)));
    std::uint32_t& an = c.s.a[(op >> 9) & 7];
    if constexpr (O == Op::Add) {
        an += src;
    } else if constexpr (O == Op::Sub) {
        an -= src;
    } else {
        arith<4, Op::Cmp>(c, src, an);
    }
    c.prefetch();
    if constexpr (O == Op::Cmp) {
        c.idle(2);
    } else if constexpr (S == 2) {
        c.idle(4);
    } else {
        c.idle(M == DReg || M == AReg || M == Imm ? 4 : 2);
    }
}

// ADDI / SUBI / CMPI #imm,<ea>
template <int S, Op O, Mode M>
void opArithImm(CpuCore& c, std::uint32_t op) {
    const std::uint32_t imm = c.immediate<S>();
    if constexpr (M == DReg) {
        std::uint32_t& d = c.s.d[op & 7];
        const std::uint32_t r = arith<S, O>(c, imm, d);
        if constexpr (O != Op::Cmp) {
            c.writeDreg<S>(op & 7, r);
        }
        c.prefetch();
        if constexpr (S == 4) {
            c.idle(O == Op::Cmp ? 2 : 4);
        }
    } else {
        const std::uint32_t addr = c.eaAddress<S, M>(op & 7);
        const std::uint32_t dst = c.read<S>(addr);
        const std::uint32_t r = arith<S, O>(c, imm, dst);
        c.prefetch();
        if constexpr (O != Op::Cmp) {
            c.write<S>(addr, r);
        }
    }
}

// ADDQ / SUBQ #1-8,<ea>
template <int S, Op O, Mode M>
void opArithQuick(CpuCore& c, std::uint32_t op) {
    std::uint32_t q = (op >> 9) & 7;
    if (q == 0) {
        q = 8;
    }
    if constexpr (M == AReg) {
        std::uint32_t& an = c.s.a[op & 7];
        an = O == Op::Add ? an + q : an - q;
        c.prefetch();
        c.idle(S == 4 ? 2 : 4);
    } else if constexpr (M == DReg) {
        const std::uint32_t r = arith<S, O>(c, q, c.s.d[op & 7]);
        c.writeDreg<S>(op & 7, r);
        c.prefetch();
        if constexpr (S == 4) {
            c.idle(4);
        }
    } else {
        const std::uint32_t addr = c.eaAddress<S, M>(op & 7);
        const std::uint32_t dst = c.read<S>(addr);
        const std::uint32_t r = arith<S, O>(c, q, dst);
        c.prefetch();
        c.write<S>(addr, r);
    }
}

// -(An) operand read for ADDX/SUBX: a long is read low word first, decrementing An by 2 before each word (an
// address error therefore leaves An 2 lower, not 4).
template <int S>
std::uint32_t readPreDec(CpuCore& c, unsigned reg) {
    std::uint32_t& an = c.s.a[reg];
    if constexpr (S == 4) {
        an -= 2;
        const std::uint32_t lo = c.readWord(an);
        an -= 2;
        return (static_cast<std::uint32_t>(c.readWord(an)) << 16) | lo;
    } else {
        an -= (S == 1 && reg != 7) ? 1 : 2;
        return c.read<S>(an);
    }
}

// ADDX / SUBX, register and -(An) forms.
template <int S, Op O, bool Memory>
void opArithX(CpuCore& c, std::uint32_t op) {
    const unsigned ry = op & 7;
    const unsigned rx = (op >> 9) & 7;
    if constexpr (!Memory) {
        const std::uint32_t r = arith<S, O>(c, c.s.d[ry], c.s.d[rx], true);
        c.writeDreg<S>(rx, r);
        c.prefetch();
        if constexpr (S == 4) {
            c.idle(4);
        }
    } else {
        c.idle(2);
        const std::uint32_t src = readPreDec<S>(c, ry);
        const std::uint32_t dst = readPreDec<S>(c, rx);
        const std::uint32_t r = arith<S, O>(c, src, dst, true);
        c.prefetch();
        if constexpr (S == 4) {
            c.writeLongDescending(c.s.a[rx], r);
        } else {
            c.write<S>(c.s.a[rx], r);
        }
    }
}

// CMPM (Ay)+,(Ax)+
template <int S>
void opCmpm(CpuCore& c, std::uint32_t op) {
    const std::uint32_t src = c.read<S>(c.eaAddress<S, PostInc>(op & 7));
    const std::uint32_t dst = c.read<S>(c.eaAddress<S, PostInc>((op >> 9) & 7));
    arith<S, Op::Cmp>(c, src, dst);
    c.prefetch();
}

// NEG / NEGX
template <int S, bool Extend, Mode M>
void opNeg(CpuCore& c, std::uint32_t op) {
    if constexpr (M == DReg) {
        const std::uint32_t r = arith<S, Op::Sub>(c, c.s.d[op & 7], 0, Extend);
        c.writeDreg<S>(op & 7, r);
        c.prefetch();
        if constexpr (S == 4) {
            c.idle(2);
        }
    } else {
        const std::uint32_t addr = c.eaAddress<S, M>(op & 7);
        const std::uint32_t v = c.read<S>(addr);
        const std::uint32_t r = arith<S, Op::Sub>(c, v, 0, Extend);
        c.prefetch();
        c.write<S>(addr, r);
    }
}

template <int S>
void opExt(CpuCore& c, std::uint32_t op) {
    std::uint32_t& d = c.dreg(op);
    if constexpr (S == 2) {
        d = (d & 0xFFFF0000u) | (static_cast<std::uint32_t>(static_cast<std::int8_t>(d & 0xFF)) & 0xFFFF);
    } else {
        d = static_cast<std::uint32_t>(static_cast<std::int16_t>(d & 0xFFFF));
    }
    c.setLogicFlags<S>(d);
    c.prefetch();
}

template <bool Signed, Mode M>
void opMul(CpuCore& c, std::uint32_t op) {
    const std::uint32_t src = c.readEa<2, M>(op & 7);
    std::uint32_t& d = c.s.d[(op >> 9) & 7];
    std::uint32_t r = 0;
    int n = 0;
    if constexpr (Signed) {
        r = static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int16_t>(src)) *
                                       static_cast<std::int16_t>(d & 0xFFFF));
        const std::uint32_t pattern = (src << 1) & 0x1FFFF;
        n = std::popcount((pattern ^ (pattern >> 1)) & 0xFFFF);
    } else {
        r = src * (d & 0xFFFF);
        n = std::popcount(src);
    }
    d = r;
    c.setLogicFlags<4>(r);
    c.prefetch();
    c.idle(34 + 2 * n);
}

// DIVU timing: 76-136 cycles depending on the quotient bits (overflow: 10).
int divuCycles(std::uint32_t dividend, std::uint16_t divisor) {
    if ((dividend >> 16) >= divisor) {
        return 10;
    }
    int cycles = 38 * 2;
    const std::uint32_t hdivisor = static_cast<std::uint32_t>(divisor) << 16;
    for (int i = 0; i < 15; ++i) {
        const std::uint32_t before = dividend;
        dividend <<= 1;
        if ((before & 0x80000000u) != 0) {
            dividend -= hdivisor;
        } else {
            cycles += 4;
            if (dividend >= hdivisor) {
                dividend -= hdivisor;
                cycles -= 2;
            }
        }
    }
    return cycles;
}

// DIVS timing: 120-158 cycles depending on signs and quotient bits (absolute overflow: 16 or 18).
int divsCycles(std::int32_t dividend, std::int16_t divisor) {
    int half = dividend < 0 ? 7 : 6;
    const std::uint32_t absDividend =
        dividend < 0 ? 0u - static_cast<std::uint32_t>(dividend) : static_cast<std::uint32_t>(dividend);
    const std::uint32_t absDivisor =
        divisor < 0 ? static_cast<std::uint32_t>(-divisor) : static_cast<std::uint32_t>(divisor);
    if ((absDividend >> 15) >= absDivisor) {  // |quotient| >= $8000: detected before the division loop
        return (half + 2) * 2;
    }
    std::uint32_t quotient = absDividend / absDivisor;
    half += 55;
    if (divisor >= 0) {
        half += dividend >= 0 ? -1 : 1;
    }
    for (int i = 0; i < 15; ++i) {
        if ((quotient & 0x8000) == 0) {
            ++half;
        }
        quotient <<= 1;
    }
    return half * 2;
}

template <bool Signed, Mode M>
void opDiv(CpuCore& c, std::uint32_t op) {
    const std::uint32_t src = c.readEa<2, M>(op & 7);
    std::uint32_t& d = c.s.d[(op >> 9) & 7];
    if (src == 0) {
        c.s.sr &= static_cast<std::uint16_t>(~(kSrCarry | kSrOverflow | kSrNegative | kSrZero));
        // The suite's only zero-divide vector stacks the address of the DIV instruction itself.
        c.exception(kVectorZeroDivide, c.instrPc, 10);
        return;
    }
    if constexpr (Signed) {
        const auto dividend = static_cast<std::int32_t>(d);
        const auto divisor = static_cast<std::int16_t>(src);
        const int cycles = divsCycles(dividend, divisor);
        const std::int64_t q = static_cast<std::int64_t>(dividend) / divisor;
        const std::int64_t rem = static_cast<std::int64_t>(dividend) % divisor;
        c.s.sr &= static_cast<std::uint16_t>(~kSrCarry);
        if (q < -32767 || q > 32767) {  // the 68000 also reports -32768 as an overflow
            c.s.sr |= kSrOverflow;      // N and Z keep their previous values
        } else {
            d = (static_cast<std::uint32_t>(rem) << 16) | (static_cast<std::uint32_t>(q) & 0xFFFF);
            c.s.sr &= static_cast<std::uint16_t>(~kSrOverflow);
            c.setNZ<2>(static_cast<std::uint32_t>(q));
        }
        c.prefetch();
        c.idle(cycles - 4);
    } else {
        const int cycles = divuCycles(d, static_cast<std::uint16_t>(src));
        const std::uint32_t q = d / src;
        const std::uint32_t rem = d % src;
        c.s.sr &= static_cast<std::uint16_t>(~kSrCarry);
        if (q > 0xFFFF) {
            c.s.sr |= kSrOverflow;  // N and Z keep their previous values
        } else {
            d = (rem << 16) | q;
            c.s.sr &= static_cast<std::uint16_t>(~kSrOverflow);
            c.setNZ<2>(q);
        }
        c.prefetch();
        c.idle(cycles - 4);
    }
}

template <int S>
constexpr std::uint16_t sizeBits() {
    return S == 1 ? 0 : S == 2 ? 1 : 2;
}

template <int S, Op O>
void registerSizedArith(TableBuilder& b, std::uint16_t line) {
    constexpr std::uint16_t ss = sizeBits<S>();
    // <ea>,Dn: line rrr0 ss ea
    b.forEach(0xF1C0, static_cast<std::uint16_t>(line | (ss << 6)), [&](std::uint32_t op) {
        const Mode m = eaMode(op);
        if (m == Invalid || (S == 1 && m == AReg)) {
            return;
        }
        b.set(op, byMode(m, []<Mode M>() -> Handler { return &opArithToReg<S, O, M>; }));
    });
    if constexpr (O != Op::Cmp) {
        // Dn,<ea>: line rrr1 ss ea (memory alterable)
        b.forEach(0xF1C0, static_cast<std::uint16_t>(line | 0x100 | (ss << 6)), [&](std::uint32_t op) {
            const Mode m = eaMode(op);
            if (!isMemoryAlterable(m)) {
                return;
            }
            b.set(op, byMode(m, []<Mode M>() -> Handler {
                      if constexpr (isMemoryAlterable(M)) {
                          return &opArithToMem<S, O, M>;
                      } else {
                          return nullptr;
                      }
                  }));
        });
        // ADDX/SUBX: line yyy1 ss00 rxxx
        b.forEach(0xF1F8, static_cast<std::uint16_t>(line | 0x100 | (ss << 6)),
                  [&](std::uint32_t op) { b.set(op, &opArithX<S, O, false>); });
        b.forEach(0xF1F8, static_cast<std::uint16_t>(line | 0x108 | (ss << 6)),
                  [&](std::uint32_t op) { b.set(op, &opArithX<S, O, true>); });
    } else {
        // CMPM: 1011 xxx1 ss00 1yyy
        b.forEach(0xF1F8, static_cast<std::uint16_t>(0xB108 | (ss << 6)),
                  [&](std::uint32_t op) { b.set(op, &opCmpm<S>); });
    }

    // ADDI 0000 0110 ss, SUBI 0000 0100 ss, CMPI 0000 1100 ss (data alterable)
    constexpr std::uint16_t immBase = O == Op::Add ? 0x0600 : O == Op::Sub ? 0x0400 : 0x0C00;
    b.forEach(0xFFC0, static_cast<std::uint16_t>(immBase | (ss << 6)), [&](std::uint32_t op) {
        const Mode m = eaMode(op);
        if (!isDataAlterable(m)) {
            return;
        }
        b.set(op, byMode(m, []<Mode M>() -> Handler {
                  if constexpr (isDataAlterable(M)) {
                      return &opArithImm<S, O, M>;
                  } else {
                      return nullptr;
                  }
              }));
    });

    if constexpr (O != Op::Cmp) {
        // ADDQ 0101 ddd0 ss ea, SUBQ 0101 ddd1 ss ea (alterable; An not for bytes)
        constexpr std::uint16_t quickBase = O == Op::Add ? 0x5000 : 0x5100;
        b.forEach(0xF1C0, static_cast<std::uint16_t>(quickBase | (ss << 6)), [&](std::uint32_t op) {
            const Mode m = eaMode(op);
            if (!isAlterable(m) || (S == 1 && m == AReg)) {
                return;
            }
            b.set(op, byMode(m, []<Mode M>() -> Handler {
                      if constexpr (isAlterable(M)) {
                          return &opArithQuick<S, O, M>;
                      } else {
                          return nullptr;
                      }
                  }));
        });
        // NEG 0100 0100 ss, NEGX 0100 0000 ss (data alterable)
        constexpr std::uint16_t negBase = O == Op::Add ? 0x4000 : 0x4400;  // Add slot registers NEGX
        b.forEach(0xFFC0, static_cast<std::uint16_t>(negBase | (ss << 6)), [&](std::uint32_t op) {
            const Mode m = eaMode(op);
            if (!isDataAlterable(m)) {
                return;
            }
            b.set(op, byMode(m, []<Mode M>() -> Handler {
                      if constexpr (isDataAlterable(M)) {
                          return &opNeg<S, O == Op::Add, M>;
                      } else {
                          return nullptr;
                      }
                  }));
        });
    }
}

template <Op O>
void registerAddressArith(TableBuilder& b, std::uint16_t line) {
    // line rrr s11 ea
    b.forEach(0xF0C0, static_cast<std::uint16_t>(line | 0x00C0), [&](std::uint32_t op) {
        const Mode m = eaMode(op);
        if (m == Invalid) {
            return;
        }
        const bool isLong = (op & 0x100) != 0;
        b.set(op, byMode(m, [&]<Mode M>() -> Handler {
                  return isLong ? &opArithAddr<4, O, M> : &opArithAddr<2, O, M>;
              }));
    });
}

template <bool Signed, bool Divide>
void registerMulDiv(TableBuilder& b) {
    const std::uint16_t base = static_cast<std::uint16_t>((Divide ? 0x80C0 : 0xC0C0) | (Signed ? 0x100 : 0));
    b.forEach(0xF1C0, base, [&](std::uint32_t op) {
        const Mode m = eaMode(op);
        if (!isData(m)) {
            return;
        }
        b.set(op, byMode(m, []<Mode M>() -> Handler {
                  if constexpr (Divide) {
                      return &opDiv<Signed, M>;
                  } else {
                      return &opMul<Signed, M>;
                  }
              }));
    });
}

}  // namespace

void registerArithmetic(TableBuilder& b) {
    registerSizedArith<1, Op::Add>(b, 0xD000);
    registerSizedArith<2, Op::Add>(b, 0xD000);
    registerSizedArith<4, Op::Add>(b, 0xD000);
    registerSizedArith<1, Op::Sub>(b, 0x9000);
    registerSizedArith<2, Op::Sub>(b, 0x9000);
    registerSizedArith<4, Op::Sub>(b, 0x9000);
    registerSizedArith<1, Op::Cmp>(b, 0xB000);
    registerSizedArith<2, Op::Cmp>(b, 0xB000);
    registerSizedArith<4, Op::Cmp>(b, 0xB000);
    registerAddressArith<Op::Add>(b, 0xD000);
    registerAddressArith<Op::Sub>(b, 0x9000);
    registerAddressArith<Op::Cmp>(b, 0xB000);
    registerMulDiv<false, false>(b);
    registerMulDiv<true, false>(b);
    registerMulDiv<false, true>(b);
    registerMulDiv<true, true>(b);
    b.forEach(0xFFF8, 0x4880, [&](std::uint32_t op) { b.set(op, &opExt<2>); });
    b.forEach(0xFFF8, 0x48C0, [&](std::uint32_t op) { b.set(op, &opExt<4>); });
}

}  // namespace gen::m68k
