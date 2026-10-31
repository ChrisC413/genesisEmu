// Shifts/rotates (register and memory forms) and BTST/BCHG/BCLR/BSET (story M01-06).

#include "core/m68k/cpu_core.hpp"

namespace gen::m68k {

namespace {

enum class Shift { As, Ls, Rox, Ro };

// Shifts `v` by `count` (0-63) bit by bit and sets the flags exactly as the 68000 does.
template <int S, Shift T, bool Left>
std::uint32_t shift(CpuCore& c, std::uint32_t v, unsigned count) {
    constexpr std::uint32_t mask = sizeMask<S>();
    constexpr std::uint32_t msb = signBit<S>();
    v &= mask;
    bool carry = false;
    bool overflow = false;
    bool x = c.flag(kSrExtend);
    for (unsigned i = 0; i < count; ++i) {
        if constexpr (Left) {
            carry = (v & msb) != 0;
            std::uint32_t n = (v << 1) & mask;
            if constexpr (T == Shift::Ro) {
                n |= carry ? 1 : 0;
            } else if constexpr (T == Shift::Rox) {
                n |= x ? 1 : 0;
                x = carry;
            } else if constexpr (T == Shift::As) {
                overflow |= ((n ^ v) & msb) != 0;
            }
            v = n;
        } else {
            carry = (v & 1) != 0;
            std::uint32_t n = v >> 1;
            if constexpr (T == Shift::Ro) {
                n |= carry ? msb : 0;
            } else if constexpr (T == Shift::Rox) {
                n |= x ? msb : 0;
                x = carry;
            } else if constexpr (T == Shift::As) {
                n |= v & msb;
            }
            v = n;
        }
    }
    std::uint16_t sr = c.s.sr & static_cast<std::uint16_t>(~(kSrCarry | kSrOverflow | kSrZero | kSrNegative));
    if constexpr (T == Shift::Rox) {
        carry = x;  // with a zero count C = X
        sr = static_cast<std::uint16_t>((sr & ~kSrExtend) | (x ? kSrExtend : 0));
    } else if constexpr (T != Shift::Ro) {
        if (count != 0) {
            sr = static_cast<std::uint16_t>((sr & ~kSrExtend) | (carry ? kSrExtend : 0));
        }
    }
    sr |= static_cast<std::uint16_t>((carry ? kSrCarry : 0) | (overflow ? kSrOverflow : 0) |
                                     (v == 0 ? kSrZero : 0) | ((v & msb) != 0 ? kSrNegative : 0));
    c.s.sr = sr;
    return v;
}

template <int S, Shift T, bool Left, bool RegisterCount>
void opShiftReg(CpuCore& c, std::uint32_t op) {
    const unsigned field = (op >> 9) & 7;
    unsigned count = 0;
    if constexpr (RegisterCount) {
        count = c.s.d[field] & 63;
    } else {
        count = field == 0 ? 8 : field;
    }
    const unsigned dn = op & 7;
    c.writeDreg<S>(dn, shift<S, T, Left>(c, c.s.d[dn], count));
    c.prefetch();
    c.idle((S == 4 ? 4 : 2) + 2 * static_cast<int>(count));
}

template <Shift T, bool Left, Mode M>
void opShiftMem(CpuCore& c, std::uint32_t op) {
    const std::uint32_t addr = c.eaAddress<2, M>(op & 7);
    const std::uint32_t r = shift<2, T, Left>(c, c.readWord(addr), 1);
    c.prefetch();
    c.writeWord(addr, static_cast<std::uint16_t>(r));
}

enum class Bit { Tst, Chg, Clr, Set };

template <Bit B>
std::uint32_t modifyBit(std::uint32_t v, std::uint32_t bitMask) {
    if constexpr (B == Bit::Chg) {
        return v ^ bitMask;
    } else if constexpr (B == Bit::Clr) {
        return v & ~bitMask;
    } else if constexpr (B == Bit::Set) {
        return v | bitMask;
    } else {
        return v;
    }
}

template <Bit B, bool Static, Mode M>
void opBit(CpuCore& c, std::uint32_t op) {
    std::uint32_t bit = 0;
    if constexpr (Static) {
        bit = c.fetchExt() & 0xFF;
    } else {
        bit = c.s.d[(op >> 9) & 7];
    }
    if constexpr (M == DReg) {
        bit &= 31;
        std::uint32_t& d = c.s.d[op & 7];
        const std::uint32_t bitMask = 1u << bit;
        c.setFlag(kSrZero, (d & bitMask) == 0);
        d = modifyBit<B>(d, bitMask);
        c.prefetch();
        if constexpr (B == Bit::Tst) {
            c.idle(2);
        } else if constexpr (B == Bit::Clr) {
            c.idle(bit < 16 ? 4 : 6);
        } else {
            c.idle(bit < 16 ? 2 : 4);
        }
    } else if constexpr (B == Bit::Tst) {
        const std::uint32_t v = c.readEa<1, M>(op & 7);
        c.setFlag(kSrZero, (v & (1u << (bit & 7))) == 0);
        c.prefetch();
        if constexpr (M == Imm) {
            c.idle(2);
        }
    } else {
        const std::uint32_t addr = c.eaAddress<1, M>(op & 7);
        const std::uint32_t v = c.readByte(addr);
        const std::uint32_t bitMask = 1u << (bit & 7);
        c.setFlag(kSrZero, (v & bitMask) == 0);
        c.prefetch();
        c.writeByte(addr, static_cast<std::uint8_t>(modifyBit<B>(v, bitMask)));
    }
}

template <int S, Shift T, bool Left>
void registerShiftReg(TableBuilder& b) {
    constexpr std::uint16_t ss = S == 1 ? 0 : S == 2 ? 1 : 2;
    const std::uint16_t base =
        static_cast<std::uint16_t>(0xE000 | (Left ? 0x100 : 0) | (ss << 6) | (static_cast<int>(T) << 3));
    b.forEach(0xF1F8, base, [&](std::uint32_t op) { b.set(op, &opShiftReg<S, T, Left, false>); });
    b.forEach(0xF1F8, static_cast<std::uint16_t>(base | 0x20),
              [&](std::uint32_t op) { b.set(op, &opShiftReg<S, T, Left, true>); });
}

template <Shift T, bool Left>
void registerShift(TableBuilder& b) {
    registerShiftReg<1, T, Left>(b);
    registerShiftReg<2, T, Left>(b);
    registerShiftReg<4, T, Left>(b);
    // memory: 1110 0tt d 11 ea (memory alterable)
    const std::uint16_t base =
        static_cast<std::uint16_t>(0xE0C0 | (static_cast<int>(T) << 9) | (Left ? 0x100 : 0));
    b.forEach(0xFFC0, base, [&](std::uint32_t op) {
        const Mode m = eaMode(op);
        if (!isMemoryAlterable(m)) {
            return;
        }
        b.set(op, byMode(m, []<Mode M>() -> Handler {
                  if constexpr (isMemoryAlterable(M)) {
                      return &opShiftMem<T, Left, M>;
                  } else {
                      return nullptr;
                  }
              }));
    });
}

template <Bit B>
void registerBit(TableBuilder& b) {
    const auto type = static_cast<std::uint16_t>(static_cast<int>(B) << 6);
    const auto allowed = [](Mode m, bool isStatic) {
        if constexpr (B == Bit::Tst) {
            return isData(m) && !(isStatic && m == Imm);
        } else {
            (void)isStatic;
            return isDataAlterable(m);
        }
    };
    // dynamic: 0000 rrr1 tt ea
    b.forEach(0xF1C0, static_cast<std::uint16_t>(0x0100 | type), [&](std::uint32_t op) {
        const Mode m = eaMode(op);
        if (!allowed(m, false)) {
            return;
        }
        b.set(op, byMode(m, []<Mode M>() -> Handler {
                  if constexpr (M == AReg || (B != Bit::Tst && !isDataAlterable(M))) {
                      return nullptr;
                  } else {
                      return &opBit<B, false, M>;
                  }
              }));
    });
    // static: 0000 1000 tt ea, bit number in the extension word
    b.forEach(0xFFC0, static_cast<std::uint16_t>(0x0800 | type), [&](std::uint32_t op) {
        const Mode m = eaMode(op);
        if (!allowed(m, true)) {
            return;
        }
        b.set(op, byMode(m, []<Mode M>() -> Handler {
                  if constexpr (M == AReg || M == Imm || (B != Bit::Tst && !isDataAlterable(M))) {
                      return nullptr;
                  } else {
                      return &opBit<B, true, M>;
                  }
              }));
    });
}

}  // namespace

void registerShiftBit(TableBuilder& b) {
    registerShift<Shift::As, false>(b);
    registerShift<Shift::As, true>(b);
    registerShift<Shift::Ls, false>(b);
    registerShift<Shift::Ls, true>(b);
    registerShift<Shift::Rox, false>(b);
    registerShift<Shift::Rox, true>(b);
    registerShift<Shift::Ro, false>(b);
    registerShift<Shift::Ro, true>(b);
    registerBit<Bit::Tst>(b);
    registerBit<Bit::Chg>(b);
    registerBit<Bit::Clr>(b);
    registerBit<Bit::Set>(b);
}

}  // namespace gen::m68k
