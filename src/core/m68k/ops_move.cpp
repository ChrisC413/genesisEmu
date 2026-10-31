// Data movement: MOVE, MOVEA, MOVEQ, MOVEM, MOVEP, LEA, PEA, EXG, SWAP, LINK, UNLK, CLR (story M01-04).

#include "core/m68k/cpu_core.hpp"

namespace gen::m68k {

namespace {

constexpr bool isRegisterOrImmediate(Mode m) {
    return m == DReg || m == AReg || m == Imm;
}

// MOVE writes its destination in the microcode order the address-error frames reveal: -(An) destinations
// prefetch before writing, and with a memory source an abs.L destination is written as soon as its high
// address word has been fetched (the low word is used straight from IRC).
template <int S, Mode Src, Mode Dst>
void opMove(CpuCore& c, std::uint32_t op) {
    const std::uint32_t v = c.readEa<S, Src>(op & 7);
    const unsigned dst = (op >> 9) & 7;
    if constexpr (Dst == DReg) {
        c.writeDreg<S>(dst, v);
        c.setLogicFlags<S>(v);
        c.prefetch();
    } else if constexpr (Dst == PreDec) {
        c.setLogicFlags<S>(v);
        c.prefetch();
        if constexpr (S == 4) {
            c.writePreDecLong(dst, v);
        } else {
            c.s.a[dst] -= (S == 1 && dst != 7) ? 1 : 2;
            c.write<S>(c.s.a[dst], v);
        }
    } else if constexpr (Dst == Ind || Dst == PostInc) {
        // (An)+ is only incremented once the write has started without an address error.
        const std::uint32_t addr = c.s.a[dst];
        c.setLogicFlags<S>(v);
        c.write<S>(addr, v);
        c.prefetch();
        if constexpr (Dst == PostInc) {
            c.s.a[dst] += (S == 1 && dst != 7) ? 1 : S == 4 ? 4 : 2;
        }
    } else if constexpr (Dst == AbsL && !isRegisterOrImmediate(Src)) {
        const std::uint32_t hi = c.fetchExt();
        const std::uint32_t addr = (hi << 16) | c.s.irc;
        c.setLogicFlags<S>(v);
        c.write<S>(addr, v);
        c.fetchExt();
        c.prefetch();
    } else {
        const std::uint32_t addr = c.eaAddress<S, Dst>(dst);
        c.setLogicFlags<S>(v);
        c.write<S>(addr, v);
        c.prefetch();
    }
}

template <int S, Mode Src>
void opMovea(CpuCore& c, std::uint32_t op) {
    const std::uint32_t v = c.readEa<S, Src>(op & 7);
    c.s.a[(op >> 9) & 7] = static_cast<std::uint32_t>(signExtend<S>(v));
    c.prefetch();
}

void opMoveq(CpuCore& c, std::uint32_t op) {
    const auto v = static_cast<std::uint32_t>(static_cast<std::int8_t>(op & 0xFF));
    c.s.d[(op >> 9) & 7] = v;
    c.setLogicFlags<4>(v);
    c.prefetch();
}

template <int S, Mode M>
void opClr(CpuCore& c, std::uint32_t op) {
    if constexpr (M == DReg) {
        c.writeDreg<S>(op & 7, 0);
        c.setLogicFlags<S>(0);
        c.prefetch();
        if constexpr (S == 4) {
            c.idle(2);
        }
    } else {
        const std::uint32_t addr = c.eaAddress<S, M>(op & 7);
        c.read<S>(addr);  // the 68000 reads before clearing
        c.setLogicFlags<S>(0);
        c.prefetch();
        c.write<S>(addr, 0);
    }
}

// LEA/PEA use the "control" EA timing: indexed modes cost 2 extra internal cycles.
template <Mode M>
std::uint32_t controlAddress(CpuCore& c, unsigned reg) {
    const std::uint32_t addr = c.eaAddress<4, M>(reg);
    if constexpr (M == Index || M == PcIndex) {
        c.idle(2);
    }
    return addr;
}

template <Mode M>
void opLea(CpuCore& c, std::uint32_t op) {
    c.s.a[(op >> 9) & 7] = controlAddress<M>(c, op & 7);
    c.prefetch();
}

template <Mode M>
void opPea(CpuCore& c, std::uint32_t op) {
    const std::uint32_t addr = controlAddress<M>(c, op & 7);
    if constexpr (M == AbsW || M == AbsL) {
        c.push32(addr);
        c.prefetch();
    } else {
        c.prefetch();
        c.push32(addr);
    }
}

void opExg(CpuCore& c, std::uint32_t op) {
    const unsigned rx = (op >> 9) & 7;
    const unsigned ry = op & 7;
    switch ((op >> 3) & 0x1F) {
        case 0x08:  // Dx,Dy
            std::swap(c.s.d[rx], c.s.d[ry]);
            break;
        case 0x09:  // Ax,Ay
            std::swap(c.s.a[rx], c.s.a[ry]);
            break;
        default:  // Dx,Ay
            std::swap(c.s.d[rx], c.s.a[ry]);
            break;
    }
    c.prefetch();
    c.idle(2);
}

void opSwap(CpuCore& c, std::uint32_t op) {
    std::uint32_t& d = c.dreg(op);
    d = (d >> 16) | (d << 16);
    c.setLogicFlags<4>(d);
    c.prefetch();
}

void opLink(CpuCore& c, std::uint32_t op) {
    const unsigned r = op & 7;
    const auto disp = static_cast<std::uint32_t>(static_cast<std::int16_t>(c.fetchExt()));
    c.s.a[7] -= 4;
    const std::uint32_t sp = c.s.a[7];
    c.write<4>(sp, c.s.a[r]);
    c.s.a[r] = sp;
    c.s.a[7] += disp;
    c.prefetch();
}

void opUnlk(CpuCore& c, std::uint32_t op) {
    const unsigned r = op & 7;
    c.s.a[7] = c.s.a[r];
    const std::uint32_t v = c.read<4>(c.s.a[7]);
    c.s.a[7] += 4;
    c.s.a[r] = v;
    c.prefetch();
}

// MOVEP: byte transfers to every other address, high byte first.
template <int S, bool ToMemory>
void opMovep(CpuCore& c, std::uint32_t op) {
    const std::uint32_t base =
        c.s.a[op & 7] + static_cast<std::uint32_t>(static_cast<std::int16_t>(c.fetchExt()));
    std::uint32_t& d = c.s.d[(op >> 9) & 7];
    constexpr int kBytes = S;
    if constexpr (ToMemory) {
        for (int i = 0; i < kBytes; ++i) {
            c.writeByte(base + 2 * static_cast<std::uint32_t>(i),
                        static_cast<std::uint8_t>(d >> (8 * (kBytes - 1 - i))));
        }
    } else {
        std::uint32_t v = 0;
        for (int i = 0; i < kBytes; ++i) {
            v = (v << 8) | c.readByte(base + 2 * static_cast<std::uint32_t>(i));
        }
        if constexpr (S == 2) {
            d = (d & 0xFFFF0000u) | v;
        } else {
            d = v;
        }
    }
    c.prefetch();
}

// MOVEM registers -> memory. The -(An) form takes a reversed mask (bit 0 = A7) and stores downwards.
template <int S, Mode M>
void opMovemToMemory(CpuCore& c, std::uint32_t op) {
    const std::uint16_t mask = c.fetchExt();
    const unsigned reg = op & 7;
    constexpr std::uint32_t kStep = S;
    if constexpr (M == PreDec) {
        std::uint32_t addr = c.s.a[reg];
        for (int i = 0; i < 16; ++i) {
            if ((mask & (1u << i)) == 0) {
                continue;
            }
            const int r = 15 - i;  // bit 0 = A7 ... bit 15 = D0
            const std::uint32_t v = r >= 8 ? c.s.a[r - 8] : c.s.d[r];
            addr -= kStep;
            if constexpr (S == 4) {
                c.writeLongDescending(addr, v);
            } else {
                c.writeWord(addr, static_cast<std::uint16_t>(v));
            }
        }
        c.s.a[reg] = addr;
    } else {
        std::uint32_t addr = 0;
        if constexpr (M == Ind) {
            addr = c.s.a[reg];
        } else {
            addr = c.eaAddress<S, M>(reg);
        }
        for (int r = 0; r < 16; ++r) {
            if ((mask & (1u << r)) == 0) {
                continue;
            }
            const std::uint32_t v = r >= 8 ? c.s.a[r - 8] : c.s.d[r];
            c.write<S>(addr, v);
            addr += kStep;
        }
    }
    c.prefetch();
}

// MOVEM memory -> registers. Words are sign-extended into all 32 bits; one extra word is read at the end.
template <int S, Mode M>
void opMovemToRegisters(CpuCore& c, std::uint32_t op) {
    const std::uint16_t mask = c.fetchExt();
    const unsigned reg = op & 7;
    constexpr std::uint32_t kStep = S;
    std::uint32_t addr = 0;
    if constexpr (M == Ind || M == PostInc) {
        addr = c.s.a[reg];
        if constexpr (M == PostInc) {
            if ((addr & 1) != 0) {
                c.s.a[reg] = addr + 2;  // what the suite expects when the first read faults
            }
        }
    } else {
        addr = c.eaAddress<S, M>(reg);
    }
    for (int r = 0; r < 16; ++r) {
        if ((mask & (1u << r)) == 0) {
            continue;
        }
        const std::uint32_t v = static_cast<std::uint32_t>(signExtend<S>(c.read<S>(addr)));
        (r >= 8 ? c.s.a[r - 8] : c.s.d[r]) = v;
        addr += kStep;
    }
    c.readWord(addr);
    if constexpr (M == PostInc) {
        c.s.a[reg] = addr;
    }
    c.prefetch();
}

template <int S, Mode Src>
Handler moveHandler(Mode dst) {
    return byMode(dst, [&]<Mode D>() -> Handler {
        if constexpr (D == AReg) {
            return S == 1 ? nullptr : &opMovea<S, Src>;
        } else if constexpr (isDataAlterable(D)) {
            return &opMove<S, Src, D>;
        } else {
            return nullptr;
        }
    });
}

template <int S>
void registerMove(TableBuilder& b, std::uint16_t line) {
    b.forEach(0xF000, line, [&](std::uint32_t op) {
        const Mode src = eaMode(op);
        const Mode dst = decodeMode((op >> 6) & 7, (op >> 9) & 7);
        if (src == Invalid || dst == Invalid || (S == 1 && src == AReg)) {
            return;
        }
        b.set(op, byMode(src, [&]<Mode M>() -> Handler { return moveHandler<S, M>(dst); }));
    });
}

template <int S>
void registerSized(TableBuilder& b) {
    // CLR: 0100 0010 ss ea
    constexpr std::uint16_t ss = S == 1 ? 0 : S == 2 ? 1 : 2;
    b.forEach(0xFFC0, static_cast<std::uint16_t>(0x4200 | (ss << 6)), [&](std::uint32_t op) {
        const Mode m = eaMode(op);
        if (isDataAlterable(m)) {
            b.set(op, byMode(m, []<Mode M>() -> Handler {
                      if constexpr (isDataAlterable(M)) {
                          return &opClr<S, M>;
                      } else {
                          return nullptr;
                      }
                  }));
        }
    });
}

}  // namespace

void registerDataMovement(TableBuilder& b) {
    registerMove<1>(b, 0x1000);
    registerMove<4>(b, 0x2000);
    registerMove<2>(b, 0x3000);
    b.forEach(0xF100, 0x7000, [&](std::uint32_t op) { b.set(op, &opMoveq); });
    registerSized<1>(b);
    registerSized<2>(b);
    registerSized<4>(b);

    // LEA 0100 rrr1 11 ea, PEA 0100 1000 01 ea (control modes)
    b.forEach(0xF1C0, 0x41C0, [&](std::uint32_t op) {
        const Mode m = eaMode(op);
        if (isControl(m)) {
            b.set(op, byMode(m, []<Mode M>() -> Handler {
                      if constexpr (isControl(M)) {
                          return &opLea<M>;
                      } else {
                          return nullptr;
                      }
                  }));
        }
    });
    b.forEach(0xFFC0, 0x4840, [&](std::uint32_t op) {
        const Mode m = eaMode(op);
        if (isControl(m)) {
            b.set(op, byMode(m, []<Mode M>() -> Handler {
                      if constexpr (isControl(M)) {
                          return &opPea<M>;
                      } else {
                          return nullptr;
                      }
                  }));
        }
    });
    b.forEach(0xFFF8, 0x4840, [&](std::uint32_t op) { b.set(op, &opSwap); });

    // EXG
    b.forEach(0xF1F8, 0xC140, [&](std::uint32_t op) { b.set(op, &opExg); });
    b.forEach(0xF1F8, 0xC148, [&](std::uint32_t op) { b.set(op, &opExg); });
    b.forEach(0xF1F8, 0xC188, [&](std::uint32_t op) { b.set(op, &opExg); });

    b.forEach(0xFFF8, 0x4E50, [&](std::uint32_t op) { b.set(op, &opLink); });
    b.forEach(0xFFF8, 0x4E58, [&](std::uint32_t op) { b.set(op, &opUnlk); });

    // MOVEP 0000 ddd1 oo 001 aaa
    b.forEach(0xF1F8, 0x0108, [&](std::uint32_t op) { b.set(op, &opMovep<2, false>); });
    b.forEach(0xF1F8, 0x0148, [&](std::uint32_t op) { b.set(op, &opMovep<4, false>); });
    b.forEach(0xF1F8, 0x0188, [&](std::uint32_t op) { b.set(op, &opMovep<2, true>); });
    b.forEach(0xF1F8, 0x01C8, [&](std::uint32_t op) { b.set(op, &opMovep<4, true>); });

    // MOVEM 0100 1d00 1s ea
    b.forEach(0xFF80, 0x4880, [&](std::uint32_t op) {
        const Mode m = eaMode(op);
        if (!(m == PreDec || (isControl(m) && isAlterable(m)))) {
            return;
        }
        const bool isLong = (op & 0x40) != 0;
        b.set(op, byMode(m, [&]<Mode M>() -> Handler {
                  if constexpr (M == PreDec || (isControl(M) && isAlterable(M))) {
                      return isLong ? &opMovemToMemory<4, M> : &opMovemToMemory<2, M>;
                  } else {
                      return nullptr;
                  }
              }));
    });
    b.forEach(0xFF80, 0x4C80, [&](std::uint32_t op) {
        const Mode m = eaMode(op);
        if (!(m == PostInc || isControl(m))) {
            return;
        }
        const bool isLong = (op & 0x40) != 0;
        b.set(op, byMode(m, [&]<Mode M>() -> Handler {
                  if constexpr (M == PostInc || isControl(M)) {
                      return isLong ? &opMovemToRegisters<4, M> : &opMovemToRegisters<2, M>;
                  } else {
                      return nullptr;
                  }
              }));
    });
}

}  // namespace gen::m68k
