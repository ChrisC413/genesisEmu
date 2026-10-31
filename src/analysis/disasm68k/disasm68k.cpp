#include "analysis/disasm68k/disasm68k.hpp"

#include <array>
#include <cstdio>

namespace gen::analysis::disasm68k {

namespace {

enum class Mode {
    DReg,
    AReg,
    Ind,
    PostInc,
    PreDec,
    Disp,
    Index,
    AbsW,
    AbsL,
    PcDisp,
    PcIndex,
    Imm,
    Invalid,
};

Mode decodeMode(unsigned mode, unsigned reg) {
    if (mode < 7) {
        return static_cast<Mode>(mode);
    }
    switch (reg) {
        case 0:
            return Mode::AbsW;
        case 1:
            return Mode::AbsL;
        case 2:
            return Mode::PcDisp;
        case 3:
            return Mode::PcIndex;
        case 4:
            return Mode::Imm;
        default:
            return Mode::Invalid;
    }
}

bool isData(Mode m) {
    return m != Mode::AReg && m != Mode::Invalid;
}
bool isMemory(Mode m) {
    return m != Mode::DReg && m != Mode::AReg && m != Mode::Invalid;
}
bool isAlterable(Mode m) {
    return m != Mode::PcDisp && m != Mode::PcIndex && m != Mode::Imm && m != Mode::Invalid;
}
bool isDataAlterable(Mode m) {
    return isData(m) && isAlterable(m);
}
bool isMemoryAlterable(Mode m) {
    return isMemory(m) && isAlterable(m);
}
bool isControl(Mode m) {
    return m == Mode::Ind || m == Mode::Disp || m == Mode::Index || m == Mode::AbsW || m == Mode::AbsL ||
           m == Mode::PcDisp || m == Mode::PcIndex;
}

constexpr std::array<const char*, 16> kConditions{"t",  "f",  "hi", "ls", "cc", "cs", "ne", "eq",
                                                  "vc", "vs", "pl", "mi", "ge", "lt", "gt", "le"};

std::string hex(std::uint32_t v, int minDigits = 1) {
    char buf[16];
    std::snprintf(buf, sizeof buf, "$%0*X", minDigits, v);
    return buf;
}

std::string signedHex(std::int32_t v) {
    return v < 0 ? "-" + hex(static_cast<std::uint32_t>(-static_cast<std::int64_t>(v)))
                 : hex(static_cast<std::uint32_t>(v));
}

char sizeChar(int size) {
    return size == 1 ? 'b' : size == 2 ? 'w' : 'l';
}

class Decoder {
public:
    Decoder(std::span<const std::uint8_t> bytes, std::uint32_t address, const SymbolResolver& symbols,
            bool probe)
        : bytes_(bytes), address_(address), symbols_(symbols), probe_(probe) {}

    Instruction run() {
        out_.address = address_;
        if (bytes_.size() < 2) {
            out_.mnemonic = "dc.b";
            out_.operands = bytes_.empty() ? "" : hex(bytes_[0], 2);
            out_.length = static_cast<std::uint8_t>(bytes_.size());
            out_.flow = FlowType::Invalid;
            return out_;
        }
        op_ = word(0);
        out_.flow = FlowType::Fallthrough;
        decode();
        if (bad_) {
            Instruction invalid;
            invalid.address = address_;
            invalid.length = 2;
            invalid.mnemonic = "dc.w";
            invalid.operands = hex(op_, 4);
            invalid.flow = FlowType::Invalid;
            return invalid;
        }
        out_.length = static_cast<std::uint8_t>(pos_);
        return out_;
    }

    bool valid() {
        run();
        return !bad_;
    }

private:
    // ---- input
    // ------------------------------------------------------------------------------------------------

    std::uint16_t word(std::size_t offset) const {
        return static_cast<std::uint16_t>((bytes_[offset] << 8) | bytes_[offset + 1]);
    }
    std::uint16_t ext() {
        if (probe_) {
            pos_ += 2;
            return 0;
        }
        if (pos_ + 2 > bytes_.size()) {
            bad_ = true;
            pos_ += 2;
            return 0;
        }
        const std::uint16_t w = word(pos_);
        pos_ += 2;
        return w;
    }
    std::uint32_t here() const { return address_ + static_cast<std::uint32_t>(pos_); }
    void invalid() { bad_ = true; }
    // An encoding the CPU executes but AS cannot reproduce byte for byte.
    void nonCanonical() {
        if (!probe_) {
            bad_ = true;
        }
    }

    // ---- output
    // -----------------------------------------------------------------------------------------------

    void emit(std::string mnemonic, std::string operands = {}) {
        out_.mnemonic = std::move(mnemonic);
        out_.operands = std::move(operands);
    }
    std::string sized(const char* base, int size) const { return std::string(base) + '.' + sizeChar(size); }

    std::string address(std::uint32_t a, int digits) {
        out_.references.push_back(a);
        if (symbols_) {
            if (auto name = symbols_(a)) {
                return *name;
            }
        }
        return hex(a, digits);
    }
    std::string target(std::uint32_t a) {
        out_.target = a;  // may be odd: the CPU then takes an address error, and AS still encodes it
        return address(a, 8);
    }

    static std::string areg(unsigned r) { return r == 7 ? "sp" : "a" + std::to_string(r); }
    static std::string dreg(unsigned r) { return "d" + std::to_string(r); }

    std::string index(std::uint16_t brief) {
        if ((brief & 0x0700) != 0) {
            nonCanonical();  // scale / full-format bits (68020+), ignored by the 68000
        }
        const unsigned r = (brief >> 12) & 7;
        return std::string((brief & 0x8000) != 0 ? areg(r) : dreg(r)) + ((brief & 0x0800) != 0 ? ".l" : ".w");
    }

    std::string immediate(int size) {
        if (size == 4) {
            const std::uint32_t hi = ext();
            return "#" + hex((hi << 16) | ext());
        }
        const std::uint16_t w = ext();
        if (size == 1 && (w & 0xFF00) != 0) {
            nonCanonical();
        }
        return "#" + hex(size == 1 ? (w & 0xFFu) : w);
    }

    // Operand text for an effective address, consuming its extension words.
    std::string ea(Mode m, unsigned reg, int size) {
        switch (m) {
            case Mode::DReg:
                return dreg(reg);
            case Mode::AReg:
                return areg(reg);
            case Mode::Ind:
                return "(" + areg(reg) + ")";
            case Mode::PostInc:
                return "(" + areg(reg) + ")+";
            case Mode::PreDec:
                return "-(" + areg(reg) + ")";
            case Mode::Disp:
                return signedHex(static_cast<std::int16_t>(ext())) + "(" + areg(reg) + ")";
            case Mode::Index: {
                const std::uint16_t brief = ext();
                return signedHex(static_cast<std::int8_t>(brief & 0xFF)) + "(" + areg(reg) + "," +
                       index(brief) + ")";
            }
            case Mode::AbsW: {
                const auto v = static_cast<std::uint32_t>(static_cast<std::int16_t>(ext()));
                return "(" + address(v, (v & 0x80000000u) != 0 ? 8 : 4) + ").w";
            }
            case Mode::AbsL: {
                const std::uint32_t hi = ext();
                return "(" + address((hi << 16) | ext(), 8) + ").l";
            }
            case Mode::PcDisp: {
                const std::uint32_t base = here();
                const std::uint32_t t = base + static_cast<std::uint32_t>(static_cast<std::int16_t>(ext()));
                return address(t, 8) + "(pc)";
            }
            case Mode::PcIndex: {
                const std::uint32_t base = here();
                const std::uint16_t brief = ext();
                const std::uint32_t t =
                    base + static_cast<std::uint32_t>(static_cast<std::int8_t>(brief & 0xFF));
                return "(" + address(t, 8) + ",pc," + index(brief) + ")";
            }
            case Mode::Imm:
                return immediate(size);
            case Mode::Invalid:
                break;
        }
        invalid();
        return {};
    }
    std::string eaOf(std::uint16_t op, int size) {
        return ea(decodeMode((op >> 3) & 7, op & 7), op & 7, size);
    }
    static Mode modeOf(std::uint16_t op) { return decodeMode((op >> 3) & 7, op & 7); }

    static int sizeField(std::uint16_t op) {  // bits 7-6: 00 byte, 01 word, 10 long, 11 none
        const unsigned s = (op >> 6) & 3;
        return s == 0 ? 1 : s == 1 ? 2 : s == 2 ? 4 : 0;
    }

    static std::string registerList(std::uint16_t mask, bool reversed) {
        std::string out;
        const auto bit = [&](int r) { return ((mask >> (reversed ? 15 - r : r)) & 1) != 0; };
        for (int bank = 0; bank < 2; ++bank) {
            int r = 0;
            while (r < 8) {
                if (!bit(bank * 8 + r)) {
                    ++r;
                    continue;
                }
                int end = r;
                while (end + 1 < 8 && bit(bank * 8 + end + 1)) {
                    ++end;
                }
                if (!out.empty()) {
                    out += '/';
                }
                const char kind = bank == 0 ? 'd' : 'a';
                out += kind + std::to_string(r);
                if (end > r) {
                    out += std::string("-") + kind + std::to_string(end);
                }
                r = end + 1;
            }
        }
        return out;
    }

    // ---- decoding
    // ---------------------------------------------------------------------------------------------

    void decode() {
        switch (op_ >> 12) {
            case 0x0:
                line0();
                break;
            case 0x1:
            case 0x2:
            case 0x3:
                move();
                break;
            case 0x4:
                line4();
                break;
            case 0x5:
                line5();
                break;
            case 0x6:
                branch();
                break;
            case 0x7:
                if ((op_ & 0x0100) != 0) {
                    invalid();
                    return;
                }
                emit("moveq.l",
                     "#" + signedHex(static_cast<std::int8_t>(op_ & 0xFF)) + "," + dreg((op_ >> 9) & 7));
                break;
            case 0x8:
                orDivSbcd();
                break;
            case 0x9:
            case 0xD:
                addSub();
                break;
            case 0xB:
                cmpEor();
                break;
            case 0xC:
                andMulAbcdExg();
                break;
            case 0xE:
                shift();
                break;
            default:  // line A, line F
                invalid();
                break;
        }
    }

    void line0() {
        static constexpr std::array<const char*, 8> kImm{"ori",   "andi", "subi", "addi",
                                                         nullptr, "eori", "cmpi", nullptr};
        static constexpr std::array<const char*, 4> kBit{"btst", "bchg", "bclr", "bset"};
        const unsigned type = (op_ >> 6) & 3;
        switch (op_) {
            case 0x003C:
            case 0x023C:
            case 0x0A3C:
                emit(std::string(kImm[(op_ >> 9) & 7]) + ".b", immediate(1) + ",ccr");
                return;
            case 0x007C:
            case 0x027C:
            case 0x0A7C:
                emit(std::string(kImm[(op_ >> 9) & 7]) + ".w", immediate(2) + ",sr");
                return;
            default:
                break;
        }
        if ((op_ & 0x0138) == 0x0108) {  // MOVEP
            const unsigned dx = (op_ >> 9) & 7;
            const int size = (op_ & 0x40) != 0 ? 4 : 2;
            const std::string mem = signedHex(static_cast<std::int16_t>(ext())) + "(" + areg(op_ & 7) + ")";
            if ((op_ & 0x80) != 0) {
                emit(sized("movep", size), dreg(dx) + "," + mem);
            } else {
                emit(sized("movep", size), mem + "," + dreg(dx));
            }
            return;
        }
        const Mode m = modeOf(op_);
        if ((op_ & 0x0100) != 0) {  // dynamic bit operation
            if (type == 0 ? !isData(m) : !isDataAlterable(m)) {
                invalid();
                return;
            }
            const int size = m == Mode::DReg ? 4 : 1;
            const std::string src = dreg((op_ >> 9) & 7);
            emit(sized(kBit[type], size), src + "," + eaOf(op_, 1));
            return;
        }
        if ((op_ & 0x0F00) == 0x0800) {  // static bit operation
            if (type == 0 ? !(isData(m) && m != Mode::Imm) : !isDataAlterable(m)) {
                invalid();
                return;
            }
            const std::uint16_t bit = ext();
            if ((bit & 0xFF00) != 0) {
                nonCanonical();
            }
            const int size = m == Mode::DReg ? 4 : 1;
            emit(sized(kBit[type], size), "#" + std::to_string(bit & 0xFF) + "," + eaOf(op_, 1));
            return;
        }
        const char* name = kImm[(op_ >> 9) & 7];
        const int size = sizeField(op_);
        if (name == nullptr || size == 0 || !isDataAlterable(m)) {
            invalid();
            return;
        }
        const std::string imm = immediate(size);
        emit(sized(name, size), imm + "," + eaOf(op_, size));
    }

    void move() {
        const unsigned line = op_ >> 12;
        const int size = line == 1 ? 1 : line == 3 ? 2 : 4;
        const Mode src = modeOf(op_);
        const Mode dst = decodeMode((op_ >> 6) & 7, (op_ >> 9) & 7);
        if (src == Mode::Invalid || (size == 1 && src == Mode::AReg)) {
            invalid();
            return;
        }
        if (dst == Mode::AReg) {
            if (size == 1) {
                invalid();
                return;
            }
            const std::string s = eaOf(op_, size);
            emit(sized("movea", size), s + "," + areg((op_ >> 9) & 7));
            return;
        }
        if (!isDataAlterable(dst)) {
            invalid();
            return;
        }
        const std::string s = eaOf(op_, size);
        emit(sized("move", size), s + "," + ea(dst, (op_ >> 9) & 7, size));
    }

    void line4() {
        const Mode m = modeOf(op_);
        const unsigned reg = op_ & 7;
        switch (op_) {
            case 0x4AFC:
                emit("illegal");
                out_.flow = FlowType::Trap;
                return;
            case 0x4E70:
                emit("reset");
                return;
            case 0x4E71:
                emit("nop");
                return;
            case 0x4E72:
                emit("stop", immediate(2));
                out_.flow = FlowType::Stop;
                return;
            case 0x4E73:
                emit("rte");
                out_.flow = FlowType::Return;
                return;
            case 0x4E75:
                emit("rts");
                out_.flow = FlowType::Return;
                return;
            case 0x4E76:
                emit("trapv");
                return;
            case 0x4E77:
                emit("rtr");
                out_.flow = FlowType::Return;
                return;
            default:
                break;
        }
        if ((op_ & 0xFFF0) == 0x4E40) {
            emit("trap", "#" + std::to_string(op_ & 15));
            out_.flow = FlowType::Trap;
            return;
        }
        if ((op_ & 0xFFF8) == 0x4E50) {
            emit("link.w", areg(reg) + ",#" + signedHex(static_cast<std::int16_t>(ext())));
            return;
        }
        if ((op_ & 0xFFF8) == 0x4E58) {
            emit("unlk", areg(reg));
            return;
        }
        if ((op_ & 0xFFF0) == 0x4E60) {
            emit("move.l", (op_ & 8) != 0 ? "usp," + areg(reg) : areg(reg) + ",usp");
            return;
        }
        if ((op_ & 0xFF80) == 0x4E80) {  // JSR / JMP
            if (!isControl(m)) {
                invalid();
                return;
            }
            const bool call = (op_ & 0x40) == 0;
            const std::string operand = eaOf(op_, 4);
            emit(call ? "jsr" : "jmp", operand);
            const bool direct = m == Mode::AbsW || m == Mode::AbsL || m == Mode::PcDisp;
            if (direct) {
                out_.target = out_.references.back();
                out_.flow = call ? FlowType::Call : FlowType::Branch;
            } else {
                out_.flow = call ? FlowType::CallIndirect : FlowType::JumpIndirect;
            }
            return;
        }
        if ((op_ & 0xF1C0) == 0x41C0) {  // LEA
            if (!isControl(m)) {
                invalid();
                return;
            }
            emit("lea.l", eaOf(op_, 4) + "," + areg((op_ >> 9) & 7));
            return;
        }
        if ((op_ & 0xF1C0) == 0x4180) {  // CHK.W
            if (!isData(m)) {
                invalid();
                return;
            }
            emit("chk.w", eaOf(op_, 2) + "," + dreg((op_ >> 9) & 7));
            return;
        }
        if ((op_ & 0x0100) != 0) {
            invalid();
            return;
        }
        const int size = sizeField(op_);
        switch ((op_ >> 8) & 0xF) {
            case 0x0:  // NEGX / MOVE from SR
            case 0x2:  // CLR
            case 0x4:  // NEG / MOVE to CCR
            case 0x6:  // NOT / MOVE to SR
            {
                static constexpr std::array<const char*, 4> kNames{"negx", "clr", "neg", "not"};
                const unsigned which = (op_ >> 9) & 3;
                if (size != 0) {
                    if (!isDataAlterable(m)) {
                        invalid();
                        return;
                    }
                    emit(sized(kNames[which], size), eaOf(op_, size));
                    return;
                }
                if (which == 0 && isDataAlterable(m)) {
                    emit("move.w", "sr," + eaOf(op_, 2));
                } else if (which == 2 && isData(m)) {
                    emit("move.w", eaOf(op_, 2) + ",ccr");
                } else if (which == 3 && isData(m)) {
                    emit("move.w", eaOf(op_, 2) + ",sr");
                } else {
                    invalid();
                }
                return;
            }
            case 0x8: {
                const unsigned sub = (op_ >> 6) & 3;
                if (sub == 0) {
                    if (!isDataAlterable(m)) {
                        invalid();
                        return;
                    }
                    emit("nbcd.b", eaOf(op_, 1));
                } else if (sub == 1) {
                    if (m == Mode::DReg) {
                        emit("swap", dreg(reg));
                    } else if (isControl(m)) {
                        emit("pea.l", eaOf(op_, 4));
                    } else {
                        invalid();
                    }
                } else if (m == Mode::DReg) {
                    emit(sub == 2 ? "ext.w" : "ext.l", dreg(reg));
                } else if (m == Mode::PreDec || (isControl(m) && isAlterable(m))) {
                    movem(false, sub == 3 ? 4 : 2, m);
                } else {
                    invalid();
                }
                return;
            }
            case 0xA:
                if (size != 0) {
                    if (!isDataAlterable(m)) {
                        invalid();
                        return;
                    }
                    emit(sized("tst", size), eaOf(op_, size));
                } else if (isDataAlterable(m)) {
                    emit("tas.b", eaOf(op_, 1));
                } else {
                    invalid();
                }
                return;
            case 0xC:
                if (((op_ >> 7) & 1) != 0 && (m == Mode::PostInc || isControl(m))) {
                    movem(true, (op_ & 0x40) != 0 ? 4 : 2, m);
                } else {
                    invalid();
                }
                return;
            default:
                invalid();
                return;
        }
    }

    void movem(bool toRegisters, int size, Mode m) {
        const std::uint16_t mask = ext();
        if (mask == 0) {
            nonCanonical();  // AS cannot express an empty register list
        }
        const std::string list = registerList(mask, !toRegisters && m == Mode::PreDec);
        const std::string operand = ea(m, op_ & 7, size);
        emit(sized("movem", size), toRegisters ? operand + "," + list : list + "," + operand);
    }

    void line5() {
        const Mode m = modeOf(op_);
        const unsigned cc = (op_ >> 8) & 15;
        if (((op_ >> 6) & 3) == 3) {
            if (m == Mode::AReg) {  // DBcc
                const std::uint32_t base = here();
                const auto disp = static_cast<std::uint32_t>(static_cast<std::int16_t>(ext()));
                emit(std::string("db") + kConditions[cc] + ".w", dreg(op_ & 7) + "," + target(base + disp));
                out_.flow = FlowType::ConditionalBranch;
                return;
            }
            if (!isDataAlterable(m)) {
                invalid();
                return;
            }
            emit(std::string("s") + kConditions[cc] + ".b", eaOf(op_, 1));
            return;
        }
        const int size = sizeField(op_);
        if (!isAlterable(m) || (size == 1 && m == Mode::AReg)) {
            invalid();
            return;
        }
        unsigned q = (op_ >> 9) & 7;
        if (q == 0) {
            q = 8;
        }
        emit(sized((op_ & 0x100) != 0 ? "subq" : "addq", size),
             "#" + std::to_string(q) + "," + eaOf(op_, size));
    }

    void branch() {
        const unsigned cc = (op_ >> 8) & 15;
        const std::uint32_t base = here();
        std::uint32_t disp = 0;
        std::string suffix;
        if ((op_ & 0xFF) == 0) {
            disp = static_cast<std::uint32_t>(static_cast<std::int16_t>(ext()));
            suffix = ".w";
        } else {
            disp = static_cast<std::uint32_t>(static_cast<std::int8_t>(op_ & 0xFF));
            suffix = ".s";
        }
        const std::string name = cc == 0 ? "bra" : cc == 1 ? "bsr" : std::string("b") + kConditions[cc];
        emit(name + suffix, target(base + disp));
        out_.flow = cc == 0 ? FlowType::Branch : cc == 1 ? FlowType::Call : FlowType::ConditionalBranch;
    }

    // OR / AND / EOR / ADD / SUB / CMP with a data register operand.
    void dataRegOp(const char* name, bool allowAddressSource) {
        const int size = sizeField(op_);
        const Mode m = modeOf(op_);
        const std::string dn = dreg((op_ >> 9) & 7);
        if ((op_ & 0x100) == 0) {
            const bool ok =
                allowAddressSource ? (m != Mode::Invalid && !(size == 1 && m == Mode::AReg)) : isData(m);
            if (!ok) {
                invalid();
                return;
            }
            emit(sized(name, size), eaOf(op_, size) + "," + dn);
        } else {
            if (!isMemoryAlterable(m)) {
                invalid();
                return;
            }
            emit(sized(name, size), dn + "," + eaOf(op_, size));
        }
    }

    // ABCD/SBCD/ADDX/SUBX register or -(An) forms.
    void extendedOp(const char* name, int size) {
        const unsigned rx = (op_ >> 9) & 7;
        const unsigned ry = op_ & 7;
        if ((op_ & 8) != 0) {
            emit(sized(name, size), "-(" + areg(ry) + "),-(" + areg(rx) + ")");
        } else {
            emit(sized(name, size), dreg(ry) + "," + dreg(rx));
        }
    }

    void orDivSbcd() {
        const Mode m = modeOf(op_);
        if (((op_ >> 6) & 3) == 3) {
            if (!isData(m)) {
                invalid();
                return;
            }
            emit((op_ & 0x100) != 0 ? "divs.w" : "divu.w", eaOf(op_, 2) + "," + dreg((op_ >> 9) & 7));
            return;
        }
        if ((op_ & 0x1F0) == 0x100) {
            extendedOp("sbcd", 1);
            return;
        }
        dataRegOp("or", false);
    }

    void addSub() {
        const bool add = (op_ >> 12) == 0xD;
        const Mode m = modeOf(op_);
        if (((op_ >> 6) & 3) == 3) {
            if (m == Mode::Invalid) {
                invalid();
                return;
            }
            const int size = (op_ & 0x100) != 0 ? 4 : 2;
            emit(sized(add ? "adda" : "suba", size), eaOf(op_, size) + "," + areg((op_ >> 9) & 7));
            return;
        }
        if ((op_ & 0x130) == 0x100) {
            extendedOp(add ? "addx" : "subx", sizeField(op_));
            return;
        }
        dataRegOp(add ? "add" : "sub", true);
    }

    void cmpEor() {
        const Mode m = modeOf(op_);
        const int size = sizeField(op_);
        if (size == 0) {
            if (m == Mode::Invalid) {
                invalid();
                return;
            }
            const int s = (op_ & 0x100) != 0 ? 4 : 2;
            emit(sized("cmpa", s), eaOf(op_, s) + "," + areg((op_ >> 9) & 7));
            return;
        }
        if ((op_ & 0x100) == 0) {
            if (m == Mode::Invalid || (size == 1 && m == Mode::AReg)) {
                invalid();
                return;
            }
            emit(sized("cmp", size), eaOf(op_, size) + "," + dreg((op_ >> 9) & 7));
            return;
        }
        if (m == Mode::AReg) {
            emit(sized("cmpm", size), "(" + areg(op_ & 7) + ")+,(" + areg((op_ >> 9) & 7) + ")+");
            return;
        }
        if (!isDataAlterable(m)) {
            invalid();
            return;
        }
        emit(sized("eor", size), dreg((op_ >> 9) & 7) + "," + eaOf(op_, size));
    }

    void andMulAbcdExg() {
        const Mode m = modeOf(op_);
        const unsigned rx = (op_ >> 9) & 7;
        const unsigned ry = op_ & 7;
        if (((op_ >> 6) & 3) == 3) {
            if (!isData(m)) {
                invalid();
                return;
            }
            emit((op_ & 0x100) != 0 ? "muls.w" : "mulu.w", eaOf(op_, 2) + "," + dreg(rx));
            return;
        }
        switch (op_ & 0x1F8) {
            case 0x100:
            case 0x108:
                extendedOp("abcd", 1);
                return;
            case 0x140:
                emit("exg.l", dreg(rx) + "," + dreg(ry));
                return;
            case 0x148:
                emit("exg.l", areg(rx) + "," + areg(ry));
                return;
            case 0x188:
                emit("exg.l", dreg(rx) + "," + areg(ry));
                return;
            default:
                break;
        }
        dataRegOp("and", false);
    }

    void shift() {
        static constexpr std::array<const char*, 4> kNames{"as", "ls", "rox", "ro"};
        const bool left = (op_ & 0x100) != 0;
        if (((op_ >> 6) & 3) == 3) {
            const Mode m = modeOf(op_);
            if ((op_ & 0x0800) != 0 || !isMemoryAlterable(m)) {
                invalid();
                return;
            }
            emit(std::string(kNames[(op_ >> 9) & 3]) + (left ? "l" : "r") + ".w", eaOf(op_, 2));
            return;
        }
        const int size = sizeField(op_);
        const unsigned count = (op_ >> 9) & 7;
        const std::string src =
            (op_ & 0x20) != 0 ? dreg(count) : "#" + std::to_string(count == 0 ? 8 : count);
        emit(sized((std::string(kNames[(op_ >> 3) & 3]) + (left ? "l" : "r")).c_str(), size),
             src + "," + dreg(op_ & 7));
    }

    std::span<const std::uint8_t> bytes_;
    std::uint32_t address_;
    const SymbolResolver& symbols_;
    bool probe_;
    std::uint16_t op_ = 0;
    std::size_t pos_ = 2;
    bool bad_ = false;
    Instruction out_;
};

}  // namespace

std::string Instruction::text() const {
    return operands.empty() ? mnemonic : mnemonic + "\t" + operands;
}

Instruction disassemble(std::span<const std::uint8_t> bytes, std::uint32_t address,
                        const SymbolResolver& symbols) {
    return Decoder(bytes, address, symbols, false).run();
}

bool isValidFirstWord(std::uint16_t opcode) {
    const std::array<std::uint8_t, 2> bytes{static_cast<std::uint8_t>(opcode >> 8),
                                            static_cast<std::uint8_t>(opcode)};
    static const SymbolResolver kNone;
    return Decoder(bytes, 0, kNone, true).valid();
}

}  // namespace gen::analysis::disasm68k
