// System / status register: MOVE to/from SR, MOVE to CCR, MOVE USP, ANDI/ORI/EORI to CCR/SR, STOP, RESET,
// TRAP, TRAPV, CHK (stories M01-09, M01-10). ILLEGAL and line A/F use the default decode-table handlers.

#include "core/m68k/cpu_core.hpp"

namespace gen::m68k {

namespace {

constexpr int kVectorChk = 6;
constexpr int kVectorTrapv = 7;
constexpr int kVectorTrapBase = 32;

template <Mode M>
void opMoveFromSr(CpuCore& c, std::uint32_t op) {
    if constexpr (M == DReg) {
        c.writeDreg<2>(op & 7, c.s.sr);
        c.prefetch();
        c.idle(2);
    } else {
        const std::uint32_t addr = c.eaAddress<2, M>(op & 7);
        c.readWord(addr);  // the 68000 reads before writing
        c.prefetch();
        c.writeWord(addr, c.s.sr);
    }
}

template <bool WholeSr, Mode M>
void opMoveToSr(CpuCore& c, std::uint32_t op) {
    if constexpr (WholeSr) {
        if (!c.s.supervisor()) {
            c.privilegeViolation();
            return;
        }
    }
    const std::uint32_t v = c.readEa<2, M>(op & 7);
    if constexpr (WholeSr) {
        c.setSr(static_cast<std::uint16_t>(v));
    } else {
        c.setCcr(v);
    }
    c.idle(4);
    c.jump(c.s.pc);  // the queue is refilled after a status register write
}

enum class Logic { And, Or, Eor };

template <Logic L, bool WholeSr>
void opLogicSr(CpuCore& c, std::uint32_t /*op*/) {
    if constexpr (WholeSr) {
        if (!c.s.supervisor()) {
            c.privilegeViolation();
            return;
        }
    }
    const std::uint16_t imm = c.fetchExt();
    const std::uint16_t mask = WholeSr ? 0xFFFF : 0x00FF;
    std::uint16_t v = c.s.sr & mask;
    const std::uint16_t operand = imm & mask;
    if constexpr (L == Logic::And) {
        v &= operand;
    } else if constexpr (L == Logic::Or) {
        v |= operand;
    } else {
        v ^= operand;
    }
    if constexpr (WholeSr) {
        c.setSr(v);
    } else {
        c.setCcr(v);
    }
    c.idle(8);
    c.jump(c.s.pc);
}

void opMoveUsp(CpuCore& c, std::uint32_t op) {
    if (!c.s.supervisor()) {
        c.privilegeViolation();
        return;
    }
    if ((op & 8) != 0) {
        c.s.a[op & 7] = c.s.usp;
    } else {
        c.s.usp = c.s.a[op & 7];
    }
    c.prefetch();
}

void opStop(CpuCore& c, std::uint32_t /*op*/) {
    if (!c.s.supervisor()) {
        c.privilegeViolation();
        return;
    }
    const std::uint16_t v = c.fetchExt();
    c.setSr(v);
    c.prefetch();
    c.cycles -= 4;  // STOP takes 4 cycles; the queue refill above stands in for the fetches done on wake-up
    c.s.stopped = true;
    c.flow = Flow::Stopped;
}

void opReset(CpuCore& c, std::uint32_t /*op*/) {
    if (!c.s.supervisor()) {
        c.privilegeViolation();
        return;
    }
    c.bus->resetDevices();
    c.idle(128);
    c.prefetch();
}

void opTrap(CpuCore& c, std::uint32_t op) {
    c.exception(kVectorTrapBase + static_cast<int>(op & 15), c.s.pc, 6);
}

void opTrapv(CpuCore& c, std::uint32_t /*op*/) {
    if (c.flag(kSrOverflow)) {
        c.exception(kVectorTrapv, c.s.pc, 6);
        return;
    }
    c.prefetch();
}

template <Mode M>
void opChk(CpuCore& c, std::uint32_t op) {
    const auto bound = static_cast<std::int16_t>(c.readEa<2, M>(op & 7));
    const auto v = static_cast<std::int16_t>(c.s.d[(op >> 9) & 7] & 0xFFFF);
    // Z, V and C are cleared; N is only touched when the instruction traps (it then reflects Dn < 0).
    c.s.sr &= static_cast<std::uint16_t>(~(kSrCarry | kSrOverflow | kSrZero));
    if (v > bound) {
        c.setFlag(kSrNegative, v < 0);
        c.exception(kVectorChk, c.s.pc, 10);
        return;
    }
    if (v < 0) {
        c.setFlag(kSrNegative, true);
        c.exception(kVectorChk, c.s.pc, 12);
        return;
    }
    c.prefetch();
    c.idle(6);
}

}  // namespace

void registerSystem(TableBuilder& b) {
    // MOVE from SR 0100 0000 11 ea (data alterable)
    b.forEach(0xFFC0, 0x40C0, [&](std::uint32_t op) {
        const Mode m = eaMode(op);
        if (!isDataAlterable(m)) {
            return;
        }
        b.set(op, byMode(m, []<Mode M>() -> Handler {
                  if constexpr (isDataAlterable(M)) {
                      return &opMoveFromSr<M>;
                  } else {
                      return nullptr;
                  }
              }));
    });
    // MOVE to CCR 0100 0100 11 ea, MOVE to SR 0100 0110 11 ea (data modes)
    for (const bool whole : {false, true}) {
        b.forEach(0xFFC0, whole ? 0x46C0 : 0x44C0, [&](std::uint32_t op) {
            const Mode m = eaMode(op);
            if (!isData(m)) {
                return;
            }
            b.set(op, byMode(m, [&]<Mode M>() -> Handler {
                      if constexpr (isData(M)) {
                          return whole ? &opMoveToSr<true, M> : &opMoveToSr<false, M>;
                      } else {
                          return nullptr;
                      }
                  }));
        });
    }
    b.set(0x003C, &opLogicSr<Logic::Or, false>);
    b.set(0x007C, &opLogicSr<Logic::Or, true>);
    b.set(0x023C, &opLogicSr<Logic::And, false>);
    b.set(0x027C, &opLogicSr<Logic::And, true>);
    b.set(0x0A3C, &opLogicSr<Logic::Eor, false>);
    b.set(0x0A7C, &opLogicSr<Logic::Eor, true>);
    b.forEach(0xFFF0, 0x4E60, [&](std::uint32_t op) { b.set(op, &opMoveUsp); });
    b.forEach(0xFFF0, 0x4E40, [&](std::uint32_t op) { b.set(op, &opTrap); });
    b.set(0x4E70, &opReset);
    b.set(0x4E72, &opStop);
    b.set(0x4E76, &opTrapv);
    // CHK 0100 rrr1 10 ea (data modes)
    b.forEach(0xF1C0, 0x4180, [&](std::uint32_t op) {
        const Mode m = eaMode(op);
        if (!isData(m)) {
            return;
        }
        b.set(op, byMode(m, []<Mode M>() -> Handler {
                  if constexpr (isData(M)) {
                      return &opChk<M>;
                  } else {
                      return nullptr;
                  }
              }));
    });
}

}  // namespace gen::m68k
