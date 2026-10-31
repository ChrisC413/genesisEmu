// BCD arithmetic: ABCD, SBCD, NBCD, including the flags real silicon produces for N and V (story M01-07).
//
// The 68000 adds/subtracts in binary and then applies decimal corrections: +/-6 when the low digit carried or
// borrowed (or exceeds 9 on addition), +/-$60 for the high digit. N is the result's top bit and V reports
// that the correction flipped bit 7 (0 -> 1 on addition, 1 -> 0 on subtraction). Z is only ever cleared.

#include "core/m68k/cpu_core.hpp"

namespace gen::m68k {

namespace {

void setBcdFlags(CpuCore& c, std::uint32_t result, bool carry, bool overflow) {
    std::uint16_t sr =
        c.s.sr & static_cast<std::uint16_t>(~(kSrCarry | kSrExtend | kSrOverflow | kSrNegative));
    if (carry) {
        sr |= kSrCarry | kSrExtend;
    }
    if (overflow) {
        sr |= kSrOverflow;
    }
    if ((result & 0x80) != 0) {
        sr |= kSrNegative;
    }
    if ((result & 0xFF) != 0) {
        sr &= static_cast<std::uint16_t>(~kSrZero);
    }
    c.s.sr = sr;
}

std::uint32_t addBcd(CpuCore& c, std::uint32_t src, std::uint32_t dst) {
    const std::uint32_t x = c.flag(kSrExtend) ? 1 : 0;
    const std::uint32_t low = (src & 0x0F) + (dst & 0x0F) + x;
    const std::uint32_t binary = (src & 0xFF) + (dst & 0xFF) + x;
    std::uint32_t result = binary;
    if (low > 9) {
        result += 6;
    }
    const bool carry = (result & 0x3F0) > 0x90;
    if (carry) {
        result += 0x60;
    }
    setBcdFlags(c, result, carry, (binary & 0x80) == 0 && (result & 0x80) != 0);
    return result & 0xFF;
}

std::uint32_t subBcd(CpuCore& c, std::uint32_t src, std::uint32_t dst) {
    const std::uint32_t x = c.flag(kSrExtend) ? 1 : 0;
    const std::uint32_t binary = (dst & 0xFF) - (src & 0xFF) - x;
    const bool lowBorrow = (dst & 0x0F) < (src & 0x0F) + x;
    std::uint32_t result = binary;
    if (lowBorrow) {
        result -= 6;
    }
    const bool carry = (binary & 0x100) != 0;
    if (carry) {
        result -= 0x60;
    }
    setBcdFlags(c, result, carry || (result & 0x100) != 0, (binary & 0x80) != 0 && (result & 0x80) == 0);
    return result & 0xFF;
}

template <bool Add, bool Memory>
void opBcd(CpuCore& c, std::uint32_t op) {
    const unsigned ry = op & 7;
    const unsigned rx = (op >> 9) & 7;
    if constexpr (!Memory) {
        const std::uint32_t r = Add ? addBcd(c, c.s.d[ry], c.s.d[rx]) : subBcd(c, c.s.d[ry], c.s.d[rx]);
        c.writeDreg<1>(rx, r);
        c.prefetch();
        c.idle(2);
    } else {
        c.idle(2);
        c.s.a[ry] -= ry == 7 ? 2 : 1;
        const std::uint32_t src = c.readByte(c.s.a[ry]);
        c.s.a[rx] -= rx == 7 ? 2 : 1;
        const std::uint32_t dst = c.readByte(c.s.a[rx]);
        const std::uint32_t r = Add ? addBcd(c, src, dst) : subBcd(c, src, dst);
        c.prefetch();
        c.writeByte(c.s.a[rx], static_cast<std::uint8_t>(r));
    }
}

template <Mode M>
void opNbcd(CpuCore& c, std::uint32_t op) {
    if constexpr (M == DReg) {
        c.writeDreg<1>(op & 7, subBcd(c, c.s.d[op & 7], 0));
        c.prefetch();
        c.idle(2);
    } else {
        const std::uint32_t addr = c.eaAddress<1, M>(op & 7);
        const std::uint32_t r = subBcd(c, c.readByte(addr), 0);
        c.prefetch();
        c.writeByte(addr, static_cast<std::uint8_t>(r));
    }
}

}  // namespace

void registerBcd(TableBuilder& b) {
    b.forEach(0xF1F8, 0xC100, [&](std::uint32_t op) { b.set(op, &opBcd<true, false>); });
    b.forEach(0xF1F8, 0xC108, [&](std::uint32_t op) { b.set(op, &opBcd<true, true>); });
    b.forEach(0xF1F8, 0x8100, [&](std::uint32_t op) { b.set(op, &opBcd<false, false>); });
    b.forEach(0xF1F8, 0x8108, [&](std::uint32_t op) { b.set(op, &opBcd<false, true>); });
    b.forEach(0xFFC0, 0x4800, [&](std::uint32_t op) {
        const Mode m = eaMode(op);
        if (!isDataAlterable(m)) {
            return;
        }
        b.set(op, byMode(m, []<Mode M>() -> Handler {
                  if constexpr (isDataAlterable(M)) {
                      return &opNbcd<M>;
                  } else {
                      return nullptr;
                  }
              }));
    });
}

}  // namespace gen::m68k
