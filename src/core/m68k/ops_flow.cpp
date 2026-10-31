// Program control: Bcc, BRA, BSR, DBcc, JMP, JSR, RTS, RTR, RTE, NOP (story M01-08).

#include "core/m68k/cpu_core.hpp"

namespace gen::m68k {

namespace {

std::uint32_t sext16(std::uint16_t v) {
    return static_cast<std::uint32_t>(static_cast<std::int16_t>(v));
}

// Bcc/BRA/BSR. During execution s.pc is the address of IRC, i.e. the opcode address + 2: the branch base.
template <bool Word>
void opBcc(CpuCore& c, std::uint32_t op) {
    const unsigned cc = (op >> 8) & 15;
    const std::uint32_t base = c.s.pc;
    const std::uint32_t disp =
        Word ? sext16(c.s.irc) : static_cast<std::uint32_t>(static_cast<std::int8_t>(op & 0xFF));
    if (cc == 1) {  // BSR
        c.idle(2);
        c.push32(Word ? base + 2 : base);
        c.jump(base + disp);
        c.flow = Flow::Call;
        return;
    }
    if (c.testCondition(cc)) {
        c.idle(2);
        c.jump(base + disp);
        c.flow = Flow::Branch;
        return;
    }
    c.idle(4);
    if constexpr (Word) {
        c.fetchExt();
    }
    c.prefetch();
}

void opDbcc(CpuCore& c, std::uint32_t op) {
    const std::uint32_t base = c.s.pc;
    if (c.testCondition((op >> 8) & 15)) {
        c.idle(4);
        c.fetchExt();
        c.prefetch();
        return;
    }
    std::uint32_t& d = c.dreg(op);
    const std::uint16_t counter = static_cast<std::uint16_t>((d & 0xFFFF) - 1);
    d = (d & 0xFFFF0000u) | counter;
    if (counter != 0xFFFF) {
        c.idle(2);
        c.jump(base + sext16(c.s.irc));
        c.flow = Flow::Branch;
        return;
    }
    c.idle(6);
    c.fetchExt();
    c.prefetch();
}

// JMP/JSR address calculation: the last extension word is used straight from IRC (the target fetch replaces
// its refill). `ret` receives the address of the next instruction.
template <Mode M>
std::uint32_t jumpTarget(CpuCore& c, unsigned reg, std::uint32_t& ret) {
    if constexpr (M == Ind) {
        ret = c.s.pc;
        return c.s.a[reg];
    } else if constexpr (M == Disp) {
        c.idle(2);
        ret = c.s.pc + 2;
        return c.s.a[reg] + sext16(c.s.irc);
    } else if constexpr (M == Index) {
        c.idle(6);
        ret = c.s.pc + 2;
        return c.indexed(c.s.a[reg], c.s.irc);
    } else if constexpr (M == AbsW) {
        c.idle(2);
        ret = c.s.pc + 2;
        return sext16(c.s.irc);
    } else if constexpr (M == AbsL) {
        const std::uint32_t hi = c.fetchExt();
        ret = c.s.pc + 2;
        return (hi << 16) | c.s.irc;
    } else if constexpr (M == PcDisp) {
        c.idle(2);
        ret = c.s.pc + 2;
        return c.s.pc + sext16(c.s.irc);
    } else {
        static_assert(M == PcIndex);
        c.idle(6);
        ret = c.s.pc + 2;
        return c.indexed(c.s.pc, c.s.irc);
    }
}

template <Mode M>
void opJmp(CpuCore& c, std::uint32_t op) {
    std::uint32_t ret = 0;
    c.jump(jumpTarget<M>(c, op & 7, ret));
    c.flow = Flow::Jump;
}

template <Mode M>
void opJsr(CpuCore& c, std::uint32_t op) {
    std::uint32_t ret = 0;
    const std::uint32_t target = jumpTarget<M>(c, op & 7, ret);
    // First word at the target is fetched before the return address is pushed (an odd target faults first).
    c.s.pc = target - 2;
    const std::uint16_t first = c.readWord(target, true);
    c.push32(ret);
    c.s.ir = first;
    c.s.irc = c.readWord(target + 2, true);
    c.s.pc = target + 2;
    c.flow = Flow::Call;
}

void opRts(CpuCore& c, std::uint32_t /*op*/) {
    c.jump(c.pop32());
    c.flow = Flow::Return;
}

void opRtr(CpuCore& c, std::uint32_t /*op*/) {
    c.setCcr(c.pop16());
    c.jump(c.pop32());
    c.flow = Flow::Return;
}

void opRte(CpuCore& c, std::uint32_t /*op*/) {
    if (!c.s.supervisor()) {
        c.privilegeViolation();
        return;
    }
    const std::uint16_t sr = c.pop16();
    const std::uint32_t pc = c.pop32();
    c.setSr(sr);
    c.jump(pc);
    c.flow = Flow::Return;
}

void opNop(CpuCore& c, std::uint32_t /*op*/) {
    c.prefetch();
}

template <Mode M>
Handler controlHandler(bool call) {
    return call ? &opJsr<M> : &opJmp<M>;
}

}  // namespace

void registerProgramControl(TableBuilder& b) {
    b.forEach(0xF000, 0x6000,
              [&](std::uint32_t op) { b.set(op, (op & 0xFF) == 0 ? &opBcc<true> : &opBcc<false>); });
    b.forEach(0xF0F8, 0x50C8, [&](std::uint32_t op) { b.set(op, &opDbcc); });
    b.forEach(0xFF80, 0x4E80, [&](std::uint32_t op) {
        const Mode m = eaMode(op);
        if (!isControl(m)) {
            return;
        }
        const bool call = (op & 0x40) == 0;
        b.set(op, byMode(m, [&]<Mode M>() -> Handler {
                  if constexpr (isControl(M)) {
                      return controlHandler<M>(call);
                  } else {
                      return nullptr;
                  }
              }));
    });
    b.set(0x4E71, &opNop);
    b.set(0x4E73, &opRte);
    b.set(0x4E75, &opRts);
    b.set(0x4E77, &opRtr);
}

}  // namespace gen::m68k
