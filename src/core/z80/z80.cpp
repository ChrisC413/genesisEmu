#include "core/z80/z80.hpp"

#include <array>

#include "core/z80/z80_tables.hpp"

namespace gen::z80 {

Z80::Z80(BusZ80& bus) : bus_(bus) {}

void Z80::reset() {
    s_.pc = 0;
    s_.i = 0;
    s_.r = 0;
    s_.im = 0;
    s_.iff1 = false;
    s_.iff2 = false;
    s_.halted = false;
    s_.eiDelay = false;
    s_.ldAir = false;
    s_.q = 0;
    s_.nmiPending = false;
    s_.wz = 0;
    s_.sp = 0xFFFF;
    s_.setAf(0xFFFF);
}

// ---- Memory
// ------------------------------------------------------------------------------------------------

std::uint8_t Z80::rd(std::uint16_t addr) {
    const std::uint8_t v = bus_.read8(addr);
    if (hooks_ != nullptr && hooks_->onAccess != nullptr) {
        hooks_->onAccess(hooks_->ctx, CpuId::Z80, AccessKind::Read, addr, 1, v, pc0_);
    }
    return v;
}

void Z80::wr(std::uint16_t addr, std::uint8_t value) {
    if (hooks_ != nullptr && hooks_->onAccess != nullptr) {
        hooks_->onAccess(hooks_->ctx, CpuId::Z80, AccessKind::Write, addr, 1, value, pc0_);
    }
    bus_.write8(addr, value);
}

std::uint8_t Z80::fetchOp() {
    const std::uint8_t v = fetchArg();
    incR();
    return v;
}

std::uint8_t Z80::fetchArg() {
    const std::uint16_t addr = s_.pc++;
    const std::uint8_t v = bus_.read8(addr);
    if (hooks_ != nullptr && hooks_->onAccess != nullptr) {
        hooks_->onAccess(hooks_->ctx, CpuId::Z80, AccessKind::Fetch, addr, 1, v, pc0_);
    }
    return v;
}

std::uint16_t Z80::fetchArg16() {
    const std::uint8_t lo = fetchArg();
    return static_cast<std::uint16_t>(lo | (fetchArg() << 8));
}

void Z80::push(std::uint16_t v) {
    wr(--s_.sp, static_cast<std::uint8_t>(v >> 8));
    wr(--s_.sp, static_cast<std::uint8_t>(v));
}

std::uint16_t Z80::pop() {
    const std::uint8_t lo = rd(s_.sp++);
    return static_cast<std::uint16_t>(lo | (rd(s_.sp++) << 8));
}

// ---- Registers
// ---------------------------------------------------------------------------------------------

std::uint8_t Z80::getRegRaw(int idx) const {
    switch (idx) {
        case 0:
            return s_.b;
        case 1:
            return s_.c;
        case 2:
            return s_.d;
        case 3:
            return s_.e;
        case 4:
            return s_.h;
        case 5:
            return s_.l;
        default:
            return s_.a;
    }
}

void Z80::setRegRaw(int idx, std::uint8_t v) {
    switch (idx) {
        case 0:
            s_.b = v;
            break;
        case 1:
            s_.c = v;
            break;
        case 2:
            s_.d = v;
            break;
        case 3:
            s_.e = v;
            break;
        case 4:
            s_.h = v;
            break;
        case 5:
            s_.l = v;
            break;
        default:
            s_.a = v;
            break;
    }
}

std::uint8_t Z80::getReg(int idx) const {
    if (mode_ != 0 && (idx == 4 || idx == 5)) {
        const std::uint16_t x = mode_ == 1 ? s_.ix : s_.iy;
        return static_cast<std::uint8_t>(idx == 4 ? x >> 8 : x);
    }
    return getRegRaw(idx);
}

void Z80::setReg(int idx, std::uint8_t v) {
    if (mode_ != 0 && (idx == 4 || idx == 5)) {
        std::uint16_t& x = mode_ == 1 ? s_.ix : s_.iy;
        x = idx == 4 ? static_cast<std::uint16_t>((x & 0x00FF) | (v << 8))
                     : static_cast<std::uint16_t>((x & 0xFF00) | v);
        return;
    }
    setRegRaw(idx, v);
}

std::uint16_t Z80::getHlx() const {
    return mode_ == 0 ? s_.hl() : mode_ == 1 ? s_.ix : s_.iy;
}

void Z80::setHlx(std::uint16_t v) {
    if (mode_ == 0) {
        s_.setHl(v);
    } else if (mode_ == 1) {
        s_.ix = v;
    } else {
        s_.iy = v;
    }
}

std::uint16_t Z80::getRp(int p) const {
    switch (p) {
        case 0:
            return s_.bc();
        case 1:
            return s_.de();
        case 2:
            return getHlx();
        default:
            return s_.sp;
    }
}

void Z80::setRp(int p, std::uint16_t v) {
    switch (p) {
        case 0:
            s_.setBc(v);
            break;
        case 1:
            s_.setDe(v);
            break;
        case 2:
            setHlx(v);
            break;
        default:
            s_.sp = v;
            break;
    }
}

std::uint16_t Z80::memAddrHl() {
    if (mode_ == 0) {
        return s_.hl();
    }
    const auto d = static_cast<std::int8_t>(fetchArg());
    const auto addr = static_cast<std::uint16_t>((mode_ == 1 ? s_.ix : s_.iy) + d);
    s_.wz = addr;
    extra_ += 8;
    return addr;
}

bool Z80::cond(int cc) const {
    switch (cc) {
        case 0:
            return (s_.f & kFlagZ) == 0;
        case 1:
            return (s_.f & kFlagZ) != 0;
        case 2:
            return (s_.f & kFlagC) == 0;
        case 3:
            return (s_.f & kFlagC) != 0;
        case 4:
            return (s_.f & kFlagP) == 0;
        case 5:
            return (s_.f & kFlagP) != 0;
        case 6:
            return (s_.f & kFlagS) == 0;
        default:
            return (s_.f & kFlagS) != 0;
    }
}

// ---- ALU
// ---------------------------------------------------------------------------------------------------

void Z80::alu(int op, std::uint8_t v) {
    const std::uint8_t a = s_.a;
    switch (op) {
        case 0:    // ADD
        case 1: {  // ADC
            const int carry = op == 1 ? (s_.f & kFlagC) : 0;
            const int res = a + v + carry;
            const auto r = static_cast<std::uint8_t>(res);
            setF(static_cast<std::uint8_t>(kSz53[r] | ((a ^ v ^ r) & kFlagH) |
                                           (((a ^ ~v) & (a ^ r) & 0x80) >> 5) | ((res >> 8) & kFlagC)));
            s_.a = r;
            break;
        }
        case 2:    // SUB
        case 3:    // SBC
        case 7: {  // CP
            const int carry = op == 3 ? (s_.f & kFlagC) : 0;
            const int res = a - v - carry;
            const auto r = static_cast<std::uint8_t>(res);
            const std::uint8_t xy = op == 7 ? v : r;  // CP takes X/Y from the operand
            setF(static_cast<std::uint8_t>((kSz53[r] & (kFlagS | kFlagZ)) | (xy & (kFlagX | kFlagY)) |
                                           ((a ^ v ^ r) & kFlagH) | (((a ^ v) & (a ^ r) & 0x80) >> 5) |
                                           kFlagN | ((res >> 8) & kFlagC)));
            if (op != 7) {
                s_.a = r;
            }
            break;
        }
        case 4:  // AND
            s_.a = static_cast<std::uint8_t>(a & v);
            setF(static_cast<std::uint8_t>(kSzp[s_.a] | kFlagH));
            break;
        case 5:  // XOR
            s_.a = static_cast<std::uint8_t>(a ^ v);
            setF(kSzp[s_.a]);
            break;
        default:  // OR
            s_.a = static_cast<std::uint8_t>(a | v);
            setF(kSzp[s_.a]);
            break;
    }
}

std::uint8_t Z80::inc8(std::uint8_t v) {
    const auto r = static_cast<std::uint8_t>(v + 1);
    setF(static_cast<std::uint8_t>((s_.f & kFlagC) | kSz53[r] | ((r & 0x0F) == 0 ? kFlagH : 0) |
                                   (r == 0x80 ? kFlagP : 0)));
    return r;
}

std::uint8_t Z80::dec8(std::uint8_t v) {
    const auto r = static_cast<std::uint8_t>(v - 1);
    setF(static_cast<std::uint8_t>((s_.f & kFlagC) | kSz53[r] | kFlagN | ((r & 0x0F) == 0x0F ? kFlagH : 0) |
                                   (r == 0x7F ? kFlagP : 0)));
    return r;
}

std::uint8_t Z80::rot(int op, std::uint8_t v) {
    std::uint8_t r = 0;
    std::uint8_t c = 0;
    switch (op) {
        case 0:  // RLC
            c = static_cast<std::uint8_t>(v >> 7);
            r = static_cast<std::uint8_t>((v << 1) | c);
            break;
        case 1:  // RRC
            c = static_cast<std::uint8_t>(v & 1);
            r = static_cast<std::uint8_t>((v >> 1) | (c << 7));
            break;
        case 2:  // RL
            c = static_cast<std::uint8_t>(v >> 7);
            r = static_cast<std::uint8_t>((v << 1) | (s_.f & kFlagC));
            break;
        case 3:  // RR
            c = static_cast<std::uint8_t>(v & 1);
            r = static_cast<std::uint8_t>((v >> 1) | ((s_.f & kFlagC) << 7));
            break;
        case 4:  // SLA
            c = static_cast<std::uint8_t>(v >> 7);
            r = static_cast<std::uint8_t>(v << 1);
            break;
        case 5:  // SRA
            c = static_cast<std::uint8_t>(v & 1);
            r = static_cast<std::uint8_t>((v >> 1) | (v & 0x80));
            break;
        case 6:  // SLL (undocumented): shifts a 1 in
            c = static_cast<std::uint8_t>(v >> 7);
            r = static_cast<std::uint8_t>((v << 1) | 1);
            break;
        default:  // SRL
            c = static_cast<std::uint8_t>(v & 1);
            r = static_cast<std::uint8_t>(v >> 1);
            break;
    }
    setF(static_cast<std::uint8_t>(kSzp[r] | c));
    return r;
}

void Z80::bit(int n, std::uint8_t v, std::uint8_t xySource) {
    const bool set = (v & (1 << n)) != 0;
    std::uint8_t f = static_cast<std::uint8_t>((s_.f & kFlagC) | kFlagH | (xySource & (kFlagX | kFlagY)));
    if (!set) {
        f |= kFlagZ | kFlagP;
    } else if (n == 7) {
        f |= kFlagS;
    }
    setF(f);
}

std::uint16_t Z80::add16(std::uint16_t a, std::uint16_t b) {
    const int res = a + b;
    s_.wz = static_cast<std::uint16_t>(a + 1);
    setF(static_cast<std::uint8_t>((s_.f & (kFlagS | kFlagZ | kFlagP)) | ((res >> 8) & (kFlagX | kFlagY)) |
                                   (((a ^ b ^ res) >> 8) & kFlagH) | ((res >> 16) & kFlagC)));
    return static_cast<std::uint16_t>(res);
}

void Z80::adc16(std::uint16_t v) {
    const std::uint16_t hl = getHlx();
    const int res = hl + v + (s_.f & kFlagC);
    const auto r = static_cast<std::uint16_t>(res);
    s_.wz = static_cast<std::uint16_t>(hl + 1);
    setF(static_cast<std::uint8_t>(((r >> 8) & (kFlagS | kFlagX | kFlagY)) | (r == 0 ? kFlagZ : 0) |
                                   (((hl ^ v ^ res) >> 8) & kFlagH) |
                                   ((~(hl ^ v) & (hl ^ res) & 0x8000) >> 13) | ((res >> 16) & kFlagC)));
    setHlx(r);
}

void Z80::sbc16(std::uint16_t v) {
    const std::uint16_t hl = getHlx();
    const int res = hl - v - (s_.f & kFlagC);
    const auto r = static_cast<std::uint16_t>(res);
    s_.wz = static_cast<std::uint16_t>(hl + 1);
    setF(static_cast<std::uint8_t>(
        ((r >> 8) & (kFlagS | kFlagX | kFlagY)) | (r == 0 ? kFlagZ : 0) | (((hl ^ v ^ res) >> 8) & kFlagH) |
        (((hl ^ v) & (hl ^ res) & 0x8000) >> 13) | kFlagN | ((res >> 16) & kFlagC)));
    setHlx(r);
}

void Z80::daa() {
    const std::uint8_t a = s_.a;
    std::uint8_t diff = 0;
    std::uint8_t carry = s_.f & kFlagC;
    if ((s_.f & kFlagH) != 0 || (a & 0x0F) > 9) {
        diff = 0x06;
    }
    if (carry != 0 || a > 0x99) {
        diff |= 0x60;
        carry = kFlagC;
    }
    std::uint8_t half = 0;
    if ((s_.f & kFlagN) != 0) {
        half = ((s_.f & kFlagH) != 0 && (a & 0x0F) < 6) ? kFlagH : 0;
        s_.a = static_cast<std::uint8_t>(a - diff);
    } else {
        half = (a & 0x0F) > 9 ? kFlagH : 0;
        s_.a = static_cast<std::uint8_t>(a + diff);
    }
    setF(static_cast<std::uint8_t>(kSzp[s_.a] | (s_.f & kFlagN) | carry | half));
}

// ---- Execution
// ---------------------------------------------------------------------------------------------

int Z80::interrupt() {
    tookIrq_ = true;
    if (s_.halted) {
        s_.halted = false;
    }
    if (s_.ldAir) {
        s_.f = static_cast<std::uint8_t>(s_.f & ~kFlagP);  // NMOS: LD A,I/R just before the acceptance
    }
    s_.ldAir = false;
    s_.eiDelay = false;
    s_.q = 0;
    incR();
    int cycles = 0;
    int level = 0;
    if (s_.nmiPending) {
        s_.nmiPending = false;
        s_.iff1 = false;
        push(s_.pc);
        s_.pc = 0x0066;
        cycles = 11;
        level = 7;
    } else {
        s_.iff1 = false;
        s_.iff2 = false;
        const std::uint8_t data = bus_.irqData();
        if (s_.im == 2) {
            push(s_.pc);
            const auto vec = static_cast<std::uint16_t>((s_.i << 8) | data);
            const std::uint8_t lo = rd(vec);
            s_.pc = static_cast<std::uint16_t>(lo | (rd(static_cast<std::uint16_t>(vec + 1)) << 8));
            cycles = 19;
        } else {
            // IM 1, or IM 0 with an RST on the bus (the Genesis supplies $FF = RST 38h).
            push(s_.pc);
            s_.pc = s_.im == 1 ? 0x0038 : static_cast<std::uint16_t>(data & 0x38);
            cycles = 13;
        }
        level = 1;
    }
    s_.wz = s_.pc;
    if (hooks_ != nullptr && hooks_->onIrq != nullptr) {
        hooks_->onIrq(hooks_->ctx, CpuId::Z80, level);
    }
    return cycles;
}

int Z80::step() {
    breakHit_ = false;
    tookIrq_ = false;
    bus_.waitCycles = 0;
    if (s_.nmiPending || (s_.irqLine && s_.iff1 && !s_.eiDelay)) {
        pc0_ = s_.pc;
        const int c = interrupt();
        const int wait = bus_.waitCycles;
        bus_.waitCycles = 0;
        return c + wait;
    }
    if (s_.halted) {
        incR();  // the CPU keeps executing NOPs (M1 cycles) without advancing PC
        s_.eiDelay = false;
        s_.ldAir = false;
        s_.q = 0;
        return 4;
    }
    pc0_ = s_.pc;
    if (hooks_ != nullptr && hooks_->onInstr != nullptr) {
        const std::uint8_t peek = bus_.read8(s_.pc);
        if (hooks_->onInstr(hooks_->ctx, CpuId::Z80, s_.pc, peek) == Action::Break) {
            breakHit_ = true;
            return 0;
        }
    }
    s_.eiDelay = false;
    s_.ldAir = false;
    flagsWritten_ = false;
    mode_ = 0;
    extra_ = 0;
    const std::uint8_t op = fetchOp();
    int cycles = 0;
    switch (op) {
        case 0xCB:
            cycles = execCb();
            break;
        case 0xED:
            cycles = execEd(fetchOp());
            break;
        case 0xDD:
        case 0xFD: {
            const std::uint8_t next = bus_.read8(s_.pc);
            if (next == 0xDD || next == 0xFD || next == 0xED) {
                cycles = 4;  // the prefix acts as a NOP; the following prefix starts a new instruction
                break;
            }
            mode_ = op == 0xDD ? 1 : 2;
            s_.q = 0;  // the prefix is an M1 cycle that leaves the flags alone (SCF/CCF see Q = 0)
            const std::uint8_t op2 = fetchOp();
            cycles = 4 + (op2 == 0xCB ? execIndexedCb() : execMain(op2));
            break;
        }
        default:
            cycles = execMain(op);
            break;
    }
    s_.q = flagsWritten_ ? s_.f : 0;
    const int wait = bus_.waitCycles;
    bus_.waitCycles = 0;
    return cycles + extra_ + wait;
}

}  // namespace gen::z80
