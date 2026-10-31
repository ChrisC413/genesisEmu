// Logic: AND/OR/EOR (+ immediates), NOT, TST, TAS, Scc (story M01-06).

#include "core/m68k/cpu_core.hpp"

namespace gen::m68k {

namespace {

enum class Logic { And, Or, Eor };

template <Logic L>
constexpr std::uint32_t apply(std::uint32_t a, std::uint32_t b) {
    if constexpr (L == Logic::And) {
        return a & b;
    } else if constexpr (L == Logic::Or) {
        return a | b;
    } else {
        return a ^ b;
    }
}

template <int S, Logic L, Mode M>
void opLogicToReg(CpuCore& c, std::uint32_t op) {
    const std::uint32_t src = c.readEa<S, M>(op & 7);
    const unsigned dn = (op >> 9) & 7;
    const std::uint32_t r = apply<L>(c.s.d[dn], src) & sizeMask<S>();
    c.writeDreg<S>(dn, r);
    c.setLogicFlags<S>(r);
    c.prefetch();
    if constexpr (S == 4) {
        c.idle(M == DReg || M == Imm ? 4 : 2);
    }
}

// Dn,<ea>: memory for AND/OR; EOR also allows a data register destination.
template <int S, Logic L, Mode M>
void opLogicToEa(CpuCore& c, std::uint32_t op) {
    const std::uint32_t src = c.s.d[(op >> 9) & 7];
    if constexpr (M == DReg) {
        const std::uint32_t r = apply<L>(c.s.d[op & 7], src) & sizeMask<S>();
        c.writeDreg<S>(op & 7, r);
        c.setLogicFlags<S>(r);
        c.prefetch();
        if constexpr (S == 4) {
            c.idle(4);
        }
    } else {
        const std::uint32_t addr = c.eaAddress<S, M>(op & 7);
        const std::uint32_t r = apply<L>(c.read<S>(addr), src) & sizeMask<S>();
        c.setLogicFlags<S>(r);
        c.prefetch();
        c.write<S>(addr, r);
    }
}

template <int S, Logic L, Mode M>
void opLogicImm(CpuCore& c, std::uint32_t op) {
    const std::uint32_t imm = c.immediate<S>();
    if constexpr (M == DReg) {
        const std::uint32_t r = apply<L>(c.s.d[op & 7], imm) & sizeMask<S>();
        c.writeDreg<S>(op & 7, r);
        c.setLogicFlags<S>(r);
        c.prefetch();
        if constexpr (S == 4) {
            c.idle(4);
        }
    } else {
        const std::uint32_t addr = c.eaAddress<S, M>(op & 7);
        const std::uint32_t r = apply<L>(c.read<S>(addr), imm) & sizeMask<S>();
        c.setLogicFlags<S>(r);
        c.prefetch();
        c.write<S>(addr, r);
    }
}

template <int S, Mode M>
void opNot(CpuCore& c, std::uint32_t op) {
    if constexpr (M == DReg) {
        const std::uint32_t r = ~c.s.d[op & 7] & sizeMask<S>();
        c.writeDreg<S>(op & 7, r);
        c.setLogicFlags<S>(r);
        c.prefetch();
        if constexpr (S == 4) {
            c.idle(2);
        }
    } else {
        const std::uint32_t addr = c.eaAddress<S, M>(op & 7);
        const std::uint32_t r = ~c.read<S>(addr) & sizeMask<S>();
        c.setLogicFlags<S>(r);
        c.prefetch();
        c.write<S>(addr, r);
    }
}

template <int S, Mode M>
void opTst(CpuCore& c, std::uint32_t op) {
    c.setLogicFlags<S>(c.readEa<S, M>(op & 7));
    c.prefetch();
}

template <Mode M>
void opTas(CpuCore& c, std::uint32_t op) {
    if constexpr (M == DReg) {
        std::uint32_t& d = c.s.d[op & 7];
        c.setLogicFlags<1>(d);
        d |= 0x80;
        c.prefetch();
    } else {
        const std::uint32_t addr = c.eaAddress<1, M>(op & 7);
        const std::uint32_t v = c.readByte(addr);
        c.setLogicFlags<1>(v);
        c.idle(2);
        if (c.tasWriteback) {
            c.writeByte(addr, static_cast<std::uint8_t>(v | 0x80));
        } else {
            c.idle(4);  // the write cycle still takes its time; the Genesis bus ignores it (C68-9)
        }
        c.prefetch();
    }
}

template <Mode M>
void opScc(CpuCore& c, std::uint32_t op) {
    const bool cond = c.testCondition((op >> 8) & 15);
    const std::uint32_t v = cond ? 0xFF : 0x00;
    if constexpr (M == DReg) {
        c.writeDreg<1>(op & 7, v);
        c.prefetch();
        if (cond) {
            c.idle(2);
        }
    } else {
        const std::uint32_t addr = c.eaAddress<1, M>(op & 7);
        c.readByte(addr);  // the 68000 reads before writing
        c.prefetch();
        c.writeByte(addr, static_cast<std::uint8_t>(v));
    }
}

template <int S>
constexpr std::uint16_t sizeBits() {
    return S == 1 ? 0 : S == 2 ? 1 : 2;
}

template <int S, Logic L>
void registerSizedLogic(TableBuilder& b) {
    constexpr std::uint16_t ss = sizeBits<S>();
    if constexpr (L != Logic::Eor) {
        const std::uint16_t line = L == Logic::And ? 0xC000 : 0x8000;
        b.forEach(0xF1C0, static_cast<std::uint16_t>(line | (ss << 6)), [&](std::uint32_t op) {
            const Mode m = eaMode(op);
            if (!isData(m)) {
                return;
            }
            b.set(op, byMode(m, []<Mode M>() -> Handler {
                      if constexpr (isData(M)) {
                          return &opLogicToReg<S, L, M>;
                      } else {
                          return nullptr;
                      }
                  }));
        });
        b.forEach(0xF1C0, static_cast<std::uint16_t>(line | 0x100 | (ss << 6)), [&](std::uint32_t op) {
            const Mode m = eaMode(op);
            if (!isMemoryAlterable(m)) {
                return;
            }
            b.set(op, byMode(m, []<Mode M>() -> Handler {
                      if constexpr (isMemoryAlterable(M)) {
                          return &opLogicToEa<S, L, M>;
                      } else {
                          return nullptr;
                      }
                  }));
        });
    } else {
        b.forEach(0xF1C0, static_cast<std::uint16_t>(0xB100 | (ss << 6)), [&](std::uint32_t op) {
            const Mode m = eaMode(op);
            if (!isDataAlterable(m)) {
                return;
            }
            b.set(op, byMode(m, []<Mode M>() -> Handler {
                      if constexpr (isDataAlterable(M)) {
                          return &opLogicToEa<S, L, M>;
                      } else {
                          return nullptr;
                      }
                  }));
        });
    }
    // ORI 0000 0000 ss, ANDI 0000 0010 ss, EORI 0000 1010 ss
    const std::uint16_t immBase = L == Logic::Or ? 0x0000 : L == Logic::And ? 0x0200 : 0x0A00;
    b.forEach(0xFFC0, static_cast<std::uint16_t>(immBase | (ss << 6)), [&](std::uint32_t op) {
        const Mode m = eaMode(op);
        if (!isDataAlterable(m)) {
            return;
        }
        b.set(op, byMode(m, []<Mode M>() -> Handler {
                  if constexpr (isDataAlterable(M)) {
                      return &opLogicImm<S, L, M>;
                  } else {
                      return nullptr;
                  }
              }));
    });
}

template <int S>
void registerSizedUnary(TableBuilder& b) {
    constexpr std::uint16_t ss = sizeBits<S>();
    b.forEach(0xFFC0, static_cast<std::uint16_t>(0x4600 | (ss << 6)), [&](std::uint32_t op) {
        const Mode m = eaMode(op);
        if (!isDataAlterable(m)) {
            return;
        }
        b.set(op, byMode(m, []<Mode M>() -> Handler {
                  if constexpr (isDataAlterable(M)) {
                      return &opNot<S, M>;
                  } else {
                      return nullptr;
                  }
              }));
    });
    b.forEach(0xFFC0, static_cast<std::uint16_t>(0x4A00 | (ss << 6)), [&](std::uint32_t op) {
        const Mode m = eaMode(op);
        if (!isDataAlterable(m)) {
            return;
        }
        b.set(op, byMode(m, []<Mode M>() -> Handler {
                  if constexpr (isDataAlterable(M)) {
                      return &opTst<S, M>;
                  } else {
                      return nullptr;
                  }
              }));
    });
}

}  // namespace

void registerLogic(TableBuilder& b) {
    registerSizedLogic<1, Logic::And>(b);
    registerSizedLogic<2, Logic::And>(b);
    registerSizedLogic<4, Logic::And>(b);
    registerSizedLogic<1, Logic::Or>(b);
    registerSizedLogic<2, Logic::Or>(b);
    registerSizedLogic<4, Logic::Or>(b);
    registerSizedLogic<1, Logic::Eor>(b);
    registerSizedLogic<2, Logic::Eor>(b);
    registerSizedLogic<4, Logic::Eor>(b);
    registerSizedUnary<1>(b);
    registerSizedUnary<2>(b);
    registerSizedUnary<4>(b);

    // TAS 0100 1010 11 ea (data alterable)
    b.forEach(0xFFC0, 0x4AC0, [&](std::uint32_t op) {
        const Mode m = eaMode(op);
        if (!isDataAlterable(m)) {
            return;
        }
        b.set(op, byMode(m, []<Mode M>() -> Handler {
                  if constexpr (isDataAlterable(M)) {
                      return &opTas<M>;
                  } else {
                      return nullptr;
                  }
              }));
    });
    // Scc 0101 cccc 11 ea (data alterable; the An form is DBcc)
    b.forEach(0xF0C0, 0x50C0, [&](std::uint32_t op) {
        const Mode m = eaMode(op);
        if (!isDataAlterable(m)) {
            return;
        }
        b.set(op, byMode(m, []<Mode M>() -> Handler {
                  if constexpr (isDataAlterable(M)) {
                      return &opScc<M>;
                  } else {
                      return nullptr;
                  }
              }));
    });
}

}  // namespace gen::m68k
