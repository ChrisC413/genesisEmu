// Z80 opcode implementations: unprefixed (also with DD/FD, where HL becomes IX/IY), CB, DDCB/FDCB and ED.
// Each handler returns the base T-states; step() adds the prefix time, the (IX+d) penalty and bus waits.

#include "core/z80/z80.hpp"
#include "core/z80/z80_tables.hpp"

namespace gen::z80 {

namespace {
constexpr std::uint8_t kXy = kFlagX | kFlagY;
}

int Z80::execMain(std::uint8_t op) {
    const int y = (op >> 3) & 7;
    const int z = op & 7;
    const int p = y >> 1;
    if (op >= 0x40 && op < 0x80) {
        if (op == 0x76) {  // HALT
            s_.halted = true;
            return 4;
        }
        if (z == 6) {  // LD r,(HL) / LD r,(IX+d): r is always the real register
            const std::uint16_t addr = memAddrHl();
            setRegRaw(y, rd(addr));
            return 7;
        }
        if (y == 6) {
            const std::uint16_t addr = memAddrHl();
            wr(addr, getRegRaw(z));
            return 7;
        }
        setReg(y, getReg(z));
        return 4;
    }
    if (op >= 0x80 && op < 0xC0) {
        if (z == 6) {
            alu(y, rd(memAddrHl()));
            return 7;
        }
        alu(y, getReg(z));
        return 4;
    }
    switch (op) {
        case 0x00:  // NOP
            return 4;
        case 0x01:
        case 0x11:
        case 0x21:
        case 0x31:  // LD rp,nn
            setRp(p, fetchArg16());
            return 10;
        case 0x02:    // LD (BC),A
        case 0x12: {  // LD (DE),A
            const std::uint16_t addr = op == 0x02 ? s_.bc() : s_.de();
            wr(addr, s_.a);
            s_.wz = static_cast<std::uint16_t>(((addr + 1) & 0xFF) | (s_.a << 8));
            return 7;
        }
        case 0x0A:    // LD A,(BC)
        case 0x1A: {  // LD A,(DE)
            const std::uint16_t addr = op == 0x0A ? s_.bc() : s_.de();
            s_.a = rd(addr);
            s_.wz = static_cast<std::uint16_t>(addr + 1);
            return 7;
        }
        case 0x22: {  // LD (nn),HL
            const std::uint16_t nn = fetchArg16();
            const std::uint16_t v = getHlx();
            wr(nn, static_cast<std::uint8_t>(v));
            wr(static_cast<std::uint16_t>(nn + 1), static_cast<std::uint8_t>(v >> 8));
            s_.wz = static_cast<std::uint16_t>(nn + 1);
            return 16;
        }
        case 0x2A: {  // LD HL,(nn)
            const std::uint16_t nn = fetchArg16();
            const std::uint8_t lo = rd(nn);
            setHlx(static_cast<std::uint16_t>(lo | (rd(static_cast<std::uint16_t>(nn + 1)) << 8)));
            s_.wz = static_cast<std::uint16_t>(nn + 1);
            return 16;
        }
        case 0x32: {  // LD (nn),A
            const std::uint16_t nn = fetchArg16();
            wr(nn, s_.a);
            s_.wz = static_cast<std::uint16_t>(((nn + 1) & 0xFF) | (s_.a << 8));
            return 13;
        }
        case 0x3A: {  // LD A,(nn)
            const std::uint16_t nn = fetchArg16();
            s_.a = rd(nn);
            s_.wz = static_cast<std::uint16_t>(nn + 1);
            return 13;
        }
        case 0x03:
        case 0x13:
        case 0x23:
        case 0x33:  // INC rp
            setRp(p, static_cast<std::uint16_t>(getRp(p) + 1));
            return 6;
        case 0x0B:
        case 0x1B:
        case 0x2B:
        case 0x3B:  // DEC rp
            setRp(p, static_cast<std::uint16_t>(getRp(p) - 1));
            return 6;
        case 0x04:
        case 0x0C:
        case 0x14:
        case 0x1C:
        case 0x24:
        case 0x2C:
        case 0x3C:  // INC r
            setReg(y, inc8(getReg(y)));
            return 4;
        case 0x34: {  // INC (HL)
            const std::uint16_t addr = memAddrHl();
            wr(addr, inc8(rd(addr)));
            return 11;
        }
        case 0x05:
        case 0x0D:
        case 0x15:
        case 0x1D:
        case 0x25:
        case 0x2D:
        case 0x3D:  // DEC r
            setReg(y, dec8(getReg(y)));
            return 4;
        case 0x35: {  // DEC (HL)
            const std::uint16_t addr = memAddrHl();
            wr(addr, dec8(rd(addr)));
            return 11;
        }
        case 0x06:
        case 0x0E:
        case 0x16:
        case 0x1E:
        case 0x26:
        case 0x2E:
        case 0x3E:  // LD r,n
            setReg(y, fetchArg());
            return 7;
        case 0x36: {  // LD (HL),n / LD (IX+d),n (19 T-states: the displacement overlaps the operand fetch)
            const std::uint16_t addr = memAddrHl();
            if (mode_ != 0) {
                extra_ -= 3;
            }
            wr(addr, fetchArg());
            return 10;
        }
        case 0x07: {  // RLCA
            const std::uint8_t a = s_.a;
            s_.a = static_cast<std::uint8_t>((a << 1) | (a >> 7));
            setF(static_cast<std::uint8_t>((s_.f & (kFlagS | kFlagZ | kFlagP)) | (s_.a & kXy) | (a >> 7)));
            return 4;
        }
        case 0x0F: {  // RRCA
            const std::uint8_t a = s_.a;
            s_.a = static_cast<std::uint8_t>((a >> 1) | (a << 7));
            setF(static_cast<std::uint8_t>((s_.f & (kFlagS | kFlagZ | kFlagP)) | (s_.a & kXy) | (a & 1)));
            return 4;
        }
        case 0x17: {  // RLA
            const std::uint8_t a = s_.a;
            s_.a = static_cast<std::uint8_t>((a << 1) | (s_.f & kFlagC));
            setF(static_cast<std::uint8_t>((s_.f & (kFlagS | kFlagZ | kFlagP)) | (s_.a & kXy) | (a >> 7)));
            return 4;
        }
        case 0x1F: {  // RRA
            const std::uint8_t a = s_.a;
            s_.a = static_cast<std::uint8_t>((a >> 1) | ((s_.f & kFlagC) << 7));
            setF(static_cast<std::uint8_t>((s_.f & (kFlagS | kFlagZ | kFlagP)) | (s_.a & kXy) | (a & 1)));
            return 4;
        }
        case 0x08: {  // EX AF,AF'
            const std::uint16_t t = s_.af();
            s_.setAf(s_.af2);
            s_.af2 = t;
            return 4;
        }
        case 0x09:
        case 0x19:
        case 0x29:
        case 0x39:  // ADD HL,rp
            setHlx(add16(getHlx(), getRp(p)));
            return 11;
        case 0x10: {  // DJNZ e
            const auto e = static_cast<std::int8_t>(fetchArg());
            if (--s_.b != 0) {
                s_.pc = static_cast<std::uint16_t>(s_.pc + e);
                s_.wz = s_.pc;
                return 13;
            }
            return 8;
        }
        case 0x18: {  // JR e
            const auto e = static_cast<std::int8_t>(fetchArg());
            s_.pc = static_cast<std::uint16_t>(s_.pc + e);
            s_.wz = s_.pc;
            return 12;
        }
        case 0x20:
        case 0x28:
        case 0x30:
        case 0x38: {  // JR cc,e
            const auto e = static_cast<std::int8_t>(fetchArg());
            if (cond(y - 4)) {
                s_.pc = static_cast<std::uint16_t>(s_.pc + e);
                s_.wz = s_.pc;
                return 12;
            }
            return 7;
        }
        case 0x27:
            daa();
            return 4;
        case 0x2F:  // CPL
            s_.a = static_cast<std::uint8_t>(~s_.a);
            setF(static_cast<std::uint8_t>((s_.f & (kFlagS | kFlagZ | kFlagP | kFlagC)) | kFlagH | kFlagN |
                                           (s_.a & kXy)));
            return 4;
        case 0x37:  // SCF: X/Y = (Q ^ F) | A
            setF(static_cast<std::uint8_t>((s_.f & (kFlagS | kFlagZ | kFlagP)) | kFlagC |
                                           (((s_.q ^ s_.f) | s_.a) & kXy)));
            return 4;
        case 0x3F:  // CCF
            setF(static_cast<std::uint8_t>((s_.f & (kFlagS | kFlagZ | kFlagP)) |
                                           ((s_.f & kFlagC) != 0 ? kFlagH : kFlagC) |
                                           (((s_.q ^ s_.f) | s_.a) & kXy)));
            return 4;
        case 0xC0:
        case 0xC8:
        case 0xD0:
        case 0xD8:
        case 0xE0:
        case 0xE8:
        case 0xF0:
        case 0xF8:  // RET cc
            if (cond(y)) {
                s_.pc = pop();
                s_.wz = s_.pc;
                return 11;
            }
            return 5;
        case 0xC1:
        case 0xD1:
        case 0xE1:  // POP rp
            setRp(p, pop());
            return 10;
        case 0xF1:  // POP AF
            s_.setAf(pop());
            return 10;
        case 0xC2:
        case 0xCA:
        case 0xD2:
        case 0xDA:
        case 0xE2:
        case 0xEA:
        case 0xF2:
        case 0xFA: {  // JP cc,nn
            const std::uint16_t nn = fetchArg16();
            s_.wz = nn;
            if (cond(y)) {
                s_.pc = nn;
            }
            return 10;
        }
        case 0xC3:  // JP nn
            s_.pc = fetchArg16();
            s_.wz = s_.pc;
            return 10;
        case 0xC4:
        case 0xCC:
        case 0xD4:
        case 0xDC:
        case 0xE4:
        case 0xEC:
        case 0xF4:
        case 0xFC: {  // CALL cc,nn
            const std::uint16_t nn = fetchArg16();
            s_.wz = nn;
            if (cond(y)) {
                push(s_.pc);
                s_.pc = nn;
                return 17;
            }
            return 10;
        }
        case 0xC5:
        case 0xD5:
        case 0xE5:  // PUSH rp
            push(getRp(p));
            return 11;
        case 0xF5:  // PUSH AF
            push(s_.af());
            return 11;
        case 0xC6:
        case 0xCE:
        case 0xD6:
        case 0xDE:
        case 0xE6:
        case 0xEE:
        case 0xF6:
        case 0xFE:  // ALU A,n
            alu(y, fetchArg());
            return 7;
        case 0xC7:
        case 0xCF:
        case 0xD7:
        case 0xDF:
        case 0xE7:
        case 0xEF:
        case 0xF7:
        case 0xFF:  // RST p
            push(s_.pc);
            s_.pc = static_cast<std::uint16_t>(y * 8);
            s_.wz = s_.pc;
            return 11;
        case 0xC9:  // RET
            s_.pc = pop();
            s_.wz = s_.pc;
            return 10;
        case 0xCD: {  // CALL nn
            const std::uint16_t nn = fetchArg16();
            s_.wz = nn;
            push(s_.pc);
            s_.pc = nn;
            return 17;
        }
        case 0xD3: {  // OUT (n),A
            const std::uint8_t n = fetchArg();
            bus_.out8(static_cast<std::uint16_t>((s_.a << 8) | n), s_.a);
            s_.wz = static_cast<std::uint16_t>(((n + 1) & 0xFF) | (s_.a << 8));
            return 11;
        }
        case 0xDB: {  // IN A,(n)
            const std::uint8_t n = fetchArg();
            const auto port = static_cast<std::uint16_t>((s_.a << 8) | n);
            s_.a = bus_.in8(port);
            s_.wz = static_cast<std::uint16_t>(port + 1);
            return 11;
        }
        case 0xD9: {  // EXX
            const std::uint16_t bc = s_.bc();
            const std::uint16_t de = s_.de();
            const std::uint16_t hl = s_.hl();
            s_.setBc(s_.bc2);
            s_.setDe(s_.de2);
            s_.setHl(s_.hl2);
            s_.bc2 = bc;
            s_.de2 = de;
            s_.hl2 = hl;
            return 4;
        }
        case 0xE3: {  // EX (SP),HL
            const std::uint8_t lo = rd(s_.sp);
            const std::uint8_t hi = rd(static_cast<std::uint16_t>(s_.sp + 1));
            const std::uint16_t v = getHlx();
            wr(static_cast<std::uint16_t>(s_.sp + 1), static_cast<std::uint8_t>(v >> 8));
            wr(s_.sp, static_cast<std::uint8_t>(v));
            const auto nv = static_cast<std::uint16_t>(lo | (hi << 8));
            setHlx(nv);
            s_.wz = nv;
            return 19;
        }
        case 0xE9:  // JP (HL)
            s_.pc = getHlx();
            return 4;
        case 0xEB: {  // EX DE,HL (never IX/IY)
            const std::uint16_t t = s_.de();
            s_.setDe(s_.hl());
            s_.setHl(t);
            return 4;
        }
        case 0xF3:  // DI
            s_.iff1 = false;
            s_.iff2 = false;
            return 4;
        case 0xFB:  // EI
            s_.iff1 = true;
            s_.iff2 = true;
            s_.eiDelay = true;
            return 4;
        case 0xF9:  // LD SP,HL
            s_.sp = getHlx();
            return 6;
        default:
            return 4;  // unreachable: prefixes are dispatched by step()
    }
}

int Z80::execCb() {
    const std::uint8_t op = fetchOp();
    const int x = op >> 6;
    const int y = (op >> 3) & 7;
    const int z = op & 7;
    if (z == 6) {
        const std::uint16_t addr = s_.hl();
        const std::uint8_t v = rd(addr);
        switch (x) {
            case 0:
                wr(addr, rot(y, v));
                return 15;
            case 1:
                bit(y, v, static_cast<std::uint8_t>(s_.wz >> 8));
                return 12;
            case 2:
                wr(addr, static_cast<std::uint8_t>(v & ~(1 << y)));
                return 15;
            default:
                wr(addr, static_cast<std::uint8_t>(v | (1 << y)));
                return 15;
        }
    }
    const std::uint8_t v = getRegRaw(z);
    switch (x) {
        case 0:
            setRegRaw(z, rot(y, v));
            break;
        case 1:
            bit(y, v, v);
            break;
        case 2:
            setRegRaw(z, static_cast<std::uint8_t>(v & ~(1 << y)));
            break;
        default:
            setRegRaw(z, static_cast<std::uint8_t>(v | (1 << y)));
            break;
    }
    return 8;
}

int Z80::execIndexedCb() {
    // DD CB d op: neither d nor op is an M1 cycle (R advanced twice, for DD and CB).
    const auto d = static_cast<std::int8_t>(fetchArg());
    const std::uint8_t op = fetchArg();
    const int x = op >> 6;
    const int y = (op >> 3) & 7;
    const int z = op & 7;
    const auto addr = static_cast<std::uint16_t>((mode_ == 1 ? s_.ix : s_.iy) + d);
    s_.wz = addr;
    const std::uint8_t v = rd(addr);
    std::uint8_t res = 0;
    switch (x) {
        case 0:
            res = rot(y, v);
            break;
        case 1:
            bit(y, v, static_cast<std::uint8_t>(addr >> 8));
            return 16;
        case 2:
            res = static_cast<std::uint8_t>(v & ~(1 << y));
            break;
        default:
            res = static_cast<std::uint8_t>(v | (1 << y));
            break;
    }
    wr(addr, res);
    if (z != 6) {
        setRegRaw(z, res);  // undocumented: the result is also copied to a register
    }
    return 19;
}

int Z80::execEd(std::uint8_t op) {
    if ((op & 0xE4) == 0xA0) {
        return blockOp(op);
    }
    if (op < 0x40 || op >= 0x80) {
        return 8;  // undefined ED opcodes act as two NOPs
    }
    const int y = (op >> 3) & 7;
    const int z = op & 7;
    const int p = y >> 1;
    switch (z) {
        case 0: {  // IN r,(C); ED 70 only sets the flags
            const std::uint16_t bc = s_.bc();
            const std::uint8_t v = bus_.in8(bc);
            s_.wz = static_cast<std::uint16_t>(bc + 1);
            setF(static_cast<std::uint8_t>((s_.f & kFlagC) | kSzp[v]));
            if (y != 6) {
                setRegRaw(y, v);
            }
            return 12;
        }
        case 1: {  // OUT (C),r; ED 71 outputs 0 (NMOS)
            const std::uint16_t bc = s_.bc();
            bus_.out8(bc, y == 6 ? 0 : getRegRaw(y));
            s_.wz = static_cast<std::uint16_t>(bc + 1);
            return 12;
        }
        case 2:
            if ((y & 1) == 0) {
                sbc16(getRp(p));
            } else {
                adc16(getRp(p));
            }
            return 15;
        case 3: {
            const std::uint16_t nn = fetchArg16();
            if ((y & 1) == 0) {  // LD (nn),rp
                const std::uint16_t v = getRp(p);
                wr(nn, static_cast<std::uint8_t>(v));
                wr(static_cast<std::uint16_t>(nn + 1), static_cast<std::uint8_t>(v >> 8));
            } else {  // LD rp,(nn)
                const std::uint8_t lo = rd(nn);
                setRp(p, static_cast<std::uint16_t>(lo | (rd(static_cast<std::uint16_t>(nn + 1)) << 8)));
            }
            s_.wz = static_cast<std::uint16_t>(nn + 1);
            return 20;
        }
        case 4: {  // NEG
            const std::uint8_t v = s_.a;
            s_.a = 0;
            alu(2, v);
            return 8;
        }
        case 5:  // RETN / RETI (both copy IFF2 to IFF1)
            s_.iff1 = s_.iff2;
            s_.pc = pop();
            s_.wz = s_.pc;
            return 14;
        case 6: {
            static constexpr std::uint8_t kModes[8] = {0, 0, 1, 2, 0, 0, 1, 2};
            s_.im = kModes[y];
            return 8;
        }
        default:
            break;
    }
    switch (y) {
        case 0:  // LD I,A
            s_.i = s_.a;
            return 9;
        case 1:  // LD R,A
            s_.r = s_.a;
            return 9;
        case 2:  // LD A,I
        case 3:  // LD A,R
            s_.a = y == 2 ? s_.i : s_.r;
            setF(static_cast<std::uint8_t>((s_.f & kFlagC) | kSz53[s_.a] | (s_.iff2 ? kFlagP : 0)));
            s_.ldAir = true;
            return 9;
        case 4: {  // RRD
            const std::uint16_t hl = s_.hl();
            const std::uint8_t v = rd(hl);
            wr(hl, static_cast<std::uint8_t>((s_.a << 4) | (v >> 4)));
            s_.a = static_cast<std::uint8_t>((s_.a & 0xF0) | (v & 0x0F));
            setF(static_cast<std::uint8_t>((s_.f & kFlagC) | kSzp[s_.a]));
            s_.wz = static_cast<std::uint16_t>(hl + 1);
            return 18;
        }
        case 5: {  // RLD
            const std::uint16_t hl = s_.hl();
            const std::uint8_t v = rd(hl);
            wr(hl, static_cast<std::uint8_t>((v << 4) | (s_.a & 0x0F)));
            s_.a = static_cast<std::uint8_t>((s_.a & 0xF0) | (v >> 4));
            setF(static_cast<std::uint8_t>((s_.f & kFlagC) | kSzp[s_.a]));
            s_.wz = static_cast<std::uint16_t>(hl + 1);
            return 18;
        }
        default:
            return 8;  // ED 77 / ED 7F: NOP
    }
}

int Z80::blockOp(std::uint8_t op) {
    const bool down = (op & 0x08) != 0;
    const bool repeat = (op & 0x10) != 0;
    const int delta = down ? -1 : 1;
    switch (op & 3) {
        case 0: {  // LDI / LDD / LDIR / LDDR
            const std::uint8_t v = rd(s_.hl());
            wr(s_.de(), v);
            s_.setHl(static_cast<std::uint16_t>(s_.hl() + delta));
            s_.setDe(static_cast<std::uint16_t>(s_.de() + delta));
            s_.setBc(static_cast<std::uint16_t>(s_.bc() - 1));
            const auto n = static_cast<std::uint8_t>(v + s_.a);
            std::uint8_t f =
                static_cast<std::uint8_t>((s_.f & (kFlagS | kFlagZ | kFlagC)) | (s_.bc() != 0 ? kFlagP : 0) |
                                          (n & kFlagX) | ((n << 4) & kFlagY));
            if (repeat && s_.bc() != 0) {
                s_.pc = static_cast<std::uint16_t>(s_.pc - 2);
                s_.wz = static_cast<std::uint16_t>(s_.pc + 1);
                f = static_cast<std::uint8_t>((f & ~kXy) | ((s_.pc >> 8) & kXy));
                setF(f);
                return 21;
            }
            setF(f);
            return 16;
        }
        case 1: {  // CPI / CPD / CPIR / CPDR
            const std::uint8_t v = rd(s_.hl());
            const auto r = static_cast<std::uint8_t>(s_.a - v);
            const std::uint8_t half = (s_.a ^ v ^ r) & kFlagH;
            s_.setHl(static_cast<std::uint16_t>(s_.hl() + delta));
            s_.setBc(static_cast<std::uint16_t>(s_.bc() - 1));
            s_.wz = static_cast<std::uint16_t>(s_.wz + delta);
            const auto n = static_cast<std::uint8_t>(r - (half != 0 ? 1 : 0));
            std::uint8_t f =
                static_cast<std::uint8_t>((s_.f & kFlagC) | kFlagN | (kSz53[r] & (kFlagS | kFlagZ)) | half |
                                          (s_.bc() != 0 ? kFlagP : 0) | (n & kFlagX) | ((n << 4) & kFlagY));
            if (repeat && s_.bc() != 0 && r != 0) {
                s_.pc = static_cast<std::uint16_t>(s_.pc - 2);
                s_.wz = static_cast<std::uint16_t>(s_.pc + 1);
                f = static_cast<std::uint8_t>((f & ~kXy) | ((s_.pc >> 8) & kXy));
                setF(f);
                return 21;
            }
            setF(f);
            return 16;
        }
        default: {  // INI / IND / INIR / INDR and OUTI / OUTD / OTIR / OTDR
            const bool out = (op & 3) == 3;
            std::uint8_t v = 0;
            int k = 0;
            if (!out) {
                const std::uint16_t bc = s_.bc();
                v = bus_.in8(bc);
                s_.wz = static_cast<std::uint16_t>(bc + delta);
                --s_.b;
                wr(s_.hl(), v);
                s_.setHl(static_cast<std::uint16_t>(s_.hl() + delta));
                k = v + ((s_.c + delta) & 0xFF);
            } else {
                v = rd(s_.hl());
                --s_.b;
                s_.wz = static_cast<std::uint16_t>(s_.bc() + delta);
                bus_.out8(s_.bc(), v);
                s_.setHl(static_cast<std::uint16_t>(s_.hl() + delta));
                k = v + s_.l;
            }
            const std::uint8_t b = s_.b;
            std::uint8_t f =
                static_cast<std::uint8_t>(kSz53[b] | ((v & 0x80) != 0 ? kFlagN : 0) |
                                          (k > 0xFF ? (kFlagH | kFlagC) : 0) | (kSzp[(k & 7) ^ b] & kFlagP));
            if (repeat && b != 0) {
                s_.pc = static_cast<std::uint16_t>(s_.pc - 2);
                s_.wz = static_cast<std::uint16_t>(s_.pc + 1);
                f = static_cast<std::uint8_t>((f & ~kXy) | ((s_.pc >> 8) & kXy));
                std::uint8_t parity = 0;
                if ((f & kFlagC) != 0) {
                    if ((v & 0x80) != 0) {
                        parity = kSzp[(b - 1) & 7];
                        f = static_cast<std::uint8_t>((f & ~kFlagH) | ((b & 0x0F) == 0x00 ? kFlagH : 0));
                    } else {
                        parity = kSzp[(b + 1) & 7];
                        f = static_cast<std::uint8_t>((f & ~kFlagH) | ((b & 0x0F) == 0x0F ? kFlagH : 0));
                    }
                } else {
                    parity = kSzp[b & 7];
                }
                f = static_cast<std::uint8_t>(f ^ ((parity & kFlagP) ^ kFlagP));
                setF(f);
                return 21;
            }
            setF(f);
            return 16;
        }
    }
}

}  // namespace gen::z80
