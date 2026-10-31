#include "analysis/disasmz80/disasmz80.hpp"

#include <cstdio>
#include <string>

namespace gen::analysis::disasmz80 {

namespace {

constexpr const char* kCc[8] = {"nz", "z", "nc", "c", "po", "pe", "p", "m"};
constexpr const char* kAlu[8] = {"add\ta,", "adc\ta,", "sub\t", "sbc\ta,", "and\t", "xor\t", "or\t", "cp\t"};
constexpr const char* kRot[8] = {"rlc", "rrc", "rl", "rr", "sla", "sra", "sli", "srl"};
constexpr const char* kBlock[4][4] = {{"ldi", "cpi", "ini", "outi"},
                                      {"ldd", "cpd", "ind", "outd"},
                                      {"ldir", "cpir", "inir", "otir"},
                                      {"lddr", "cpdr", "indr", "otdr"}};

std::string hex(unsigned v, int digits) {
    char buf[16];
    std::snprintf(buf, sizeof buf, "$%0*X", digits, v);
    return buf;
}

struct Missing {};  // thrown when the instruction runs past the supplied bytes

class Decoder {
public:
    Decoder(std::span<const std::uint8_t> bytes, std::uint16_t address, const SymbolResolver& symbols)
        : bytes_(bytes), address_(address), symbols_(symbols) {}

    Instruction run() {
        try {
            decode();
        } catch (const Missing&) {
            pos_ = 1;
            data(1);
        }
        ins_.address = address_;
        ins_.length = static_cast<std::uint8_t>(pos_);
        return ins_;
    }

private:
    std::uint8_t next() {
        if (pos_ >= bytes_.size()) {
            throw Missing{};
        }
        return bytes_[pos_++];
    }
    std::uint16_t next16() {
        const std::uint8_t lo = next();
        return static_cast<std::uint16_t>(lo | (next() << 8));
    }
    std::string addr(std::uint16_t a) {
        ins_.references.push_back(a);
        if (symbols_) {
            if (auto name = symbols_(a)) {
                return *name;
            }
        }
        return hex(a, 4);
    }
    void set(std::string text, FlowType flow = FlowType::Fallthrough) {
        const std::size_t tab = text.find('\t');
        ins_.mnemonic = text.substr(0, tab);
        ins_.operands = tab == std::string::npos ? "" : text.substr(tab + 1);
        ins_.flow = flow;
    }
    void target(std::uint16_t t, FlowType flow) {
        ins_.target = t;
        ins_.flow = flow;
    }
    // `db` of the first n bytes (pos_ is set to n).
    void data(std::size_t n) {
        std::string ops;
        for (std::size_t i = 0; i < n; ++i) {
            ops += (i != 0 ? "," : "") + hex(bytes_[i], 2);
        }
        pos_ = n;
        ins_ = Instruction{};
        ins_.mnemonic = "db";
        ins_.operands = ops;
        ins_.flow = FlowType::Invalid;
    }

    std::string hlx() const { return ix_ == 0 ? "hl" : ix_ == 1 ? "ix" : "iy"; }
    std::string idx(std::int8_t d) const {
        const std::string sign = d < 0 ? "-" : "+";
        const int mag = d < 0 ? -d : d;
        return "(" + hlx() + sign + hex(static_cast<unsigned>(mag), 2) + ")";
    }
    // r[i] with the index prefix applied to H / L; (HL) becomes (IX+d) and fetches d.
    std::string reg(int i) {
        static constexpr const char* kR[8] = {"b", "c", "d", "e", "h", "l", "(hl)", "a"};
        if (ix_ != 0) {
            if (i == 4) {
                usesIndex_ = true;
                return hlx() + "h";
            }
            if (i == 5) {
                usesIndex_ = true;
                return hlx() + "l";
            }
            if (i == 6) {
                usesIndex_ = true;
                return idx(static_cast<std::int8_t>(next()));
            }
        }
        return kR[i];
    }
    std::string rp(int p) {
        static constexpr const char* kRp[4] = {"bc", "de", "hl", "sp"};
        if (p == 2) {
            usesIndex_ = ix_ != 0;
            return hlx();
        }
        return kRp[p];
    }
    std::string rp2(int p) { return p == 3 ? "af" : rp(p); }

    void decode() {
        const std::uint8_t op = next();
        if (op == 0xDD || op == 0xFD) {
            ix_ = op == 0xDD ? 1 : 2;
            const std::uint8_t op2 = next();
            if (op2 == 0xDD || op2 == 0xFD || op2 == 0xED) {
                data(1);  // the prefix only costs time; AS cannot emit it alone
                return;
            }
            if (op2 == 0xCB) {
                indexedCb();
                return;
            }
            main(op2);
            if (!usesIndex_) {
                data(1);
            }
            return;
        }
        if (op == 0xCB) {
            cb();
            return;
        }
        if (op == 0xED) {
            ed();
            return;
        }
        main(op);
    }

    void main(std::uint8_t op) {
        const int x = op >> 6;
        const int y = (op >> 3) & 7;
        const int z = op & 7;
        const int p = y >> 1;
        const int q = y & 1;
        const auto rel = [&] {
            const auto d = static_cast<std::int8_t>(next());
            return static_cast<std::uint16_t>(address_ + pos_ + d);
        };
        if (x == 1) {
            if (y == 6 && z == 6) {
                set("halt", FlowType::Stop);
                return;
            }
            // With (IX+d) the other operand is the real H / L.
            if (z == 6 || y == 6) {
                const bool memSrc = z == 6;
                std::string mem = reg(6);
                const int other = memSrc ? y : z;
                static constexpr const char* kR[8] = {"b", "c", "d", "e", "h", "l", "(hl)", "a"};
                set(memSrc ? "ld\t" + std::string(kR[other]) + "," + mem : "ld\t" + mem + "," + kR[other]);
                return;
            }
            const std::string dst = reg(y);
            set("ld\t" + dst + "," + reg(z));
            return;
        }
        if (x == 2) {
            set(std::string(kAlu[y]) + reg(z));
            return;
        }
        if (x == 0) {
            switch (z) {
                case 0:
                    if (y == 0) {
                        set("nop");
                    } else if (y == 1) {
                        set("ex\taf,af'");
                    } else if (y == 2) {
                        const std::uint16_t t = rel();
                        set("djnz\t" + addr(t));
                        target(t, FlowType::ConditionalBranch);
                    } else if (y == 3) {
                        const std::uint16_t t = rel();
                        set("jr\t" + addr(t));
                        target(t, FlowType::Branch);
                    } else {
                        const std::uint16_t t = rel();
                        set("jr\t" + std::string(kCc[y - 4]) + "," + addr(t));
                        target(t, FlowType::ConditionalBranch);
                    }
                    return;
                case 1:
                    if (q == 0) {
                        const std::string r = rp(p);
                        set("ld\t" + r + "," + hex(next16(), 4));
                    } else {
                        const std::string dst = hlx();
                        usesIndex_ = ix_ != 0;
                        set("add\t" + dst + "," + rp(p));
                    }
                    return;
                case 2: {
                    static constexpr const char* kInd[2] = {"(bc)", "(de)"};
                    if (p < 2) {
                        set(q == 0 ? "ld\t" + std::string(kInd[p]) + ",a" : "ld\ta," + std::string(kInd[p]));
                    } else if (p == 2) {
                        const std::string r = rp(2);
                        const std::string m = "(" + addr(next16()) + ")";
                        set(q == 0 ? "ld\t" + m + "," + r : "ld\t" + r + "," + m);
                    } else {
                        const std::string m = "(" + addr(next16()) + ")";
                        set(q == 0 ? "ld\t" + m + ",a" : "ld\ta," + m);
                    }
                    return;
                }
                case 3:
                    set(std::string(q == 0 ? "inc\t" : "dec\t") + rp(p));
                    return;
                case 4:
                    set("inc\t" + reg(y));
                    return;
                case 5:
                    set("dec\t" + reg(y));
                    return;
                case 6: {
                    const std::string r = reg(y);
                    set("ld\t" + r + "," + hex(next(), 2));
                    return;
                }
                default: {
                    static constexpr const char* kAcc[8] = {"rlca", "rrca", "rla", "rra",
                                                            "daa",  "cpl",  "scf", "ccf"};
                    set(kAcc[y]);
                    return;
                }
            }
        }
        // x == 3
        switch (z) {
            case 0:
                set("ret\t" + std::string(kCc[y]), FlowType::ConditionalBranch);
                return;
            case 1:
                if (q == 0) {
                    set("pop\t" + rp2(p));
                } else if (p == 0) {
                    set("ret", FlowType::Return);
                } else if (p == 1) {
                    set("exx");
                } else if (p == 2) {
                    set("jp\t(" + rp(2) + ")", FlowType::JumpIndirect);
                } else {
                    const std::string r = rp(2);
                    set("ld\tsp," + r);
                }
                return;
            case 2: {
                const std::uint16_t t = next16();
                set("jp\t" + std::string(kCc[y]) + "," + addr(t));
                target(t, FlowType::ConditionalBranch);
                return;
            }
            case 3:
                switch (y) {
                    case 0: {
                        const std::uint16_t t = next16();
                        set("jp\t" + addr(t));
                        target(t, FlowType::Branch);
                        return;
                    }
                    case 2:
                        set("out\t(" + hex(next(), 2) + "),a");
                        return;
                    case 3:
                        set("in\ta,(" + hex(next(), 2) + ")");
                        return;
                    case 4:
                        set("ex\t(sp)," + rp(2));
                        return;
                    case 5:
                        set("ex\tde,hl");
                        return;
                    case 6:
                        set("di");
                        return;
                    default:
                        set("ei");
                        return;
                }
            case 4: {
                const std::uint16_t t = next16();
                set("call\t" + std::string(kCc[y]) + "," + addr(t));
                target(t, FlowType::Call);
                return;
            }
            case 5:
                if (q == 0) {
                    set("push\t" + rp2(p));
                } else {
                    const std::uint16_t t = next16();  // p == 0 (prefixes are handled by decode())
                    set("call\t" + addr(t));
                    target(t, FlowType::Call);
                }
                return;
            case 6:
                set(std::string(kAlu[y]) + hex(next(), 2));
                return;
            default:
                set("rst\t" + hex(static_cast<unsigned>(y * 8), 2));
                target(static_cast<std::uint16_t>(y * 8), FlowType::Call);
                return;
        }
    }

    void cb() {
        const std::uint8_t op = next();
        const int x = op >> 6;
        const int y = (op >> 3) & 7;
        const std::string r = reg(op & 7);
        if (x == 0) {
            set(std::string(kRot[y]) + "\t" + r);
        } else {
            static constexpr const char* kBit[4] = {"", "bit", "res", "set"};
            set(std::string(kBit[x]) + "\t" + std::to_string(y) + "," + r);
        }
    }

    void indexedCb() {
        const auto d = static_cast<std::int8_t>(next());
        const std::uint8_t op = next();
        const int x = op >> 6;
        const int y = (op >> 3) & 7;
        const int z = op & 7;
        static constexpr const char* kR[8] = {"b", "c", "d", "e", "h", "l", "", "a"};
        const std::string mem = idx(d);
        const std::string copy = z == 6 ? "" : "," + std::string(kR[z]);  // undocumented register copy
        if (x == 0) {
            set(std::string(kRot[y]) + "\t" + mem + copy);
        } else if (x == 1) {
            if (z != 6) {
                data(4);  // BIT ignores the register field; AS emits the z = 6 form
                return;
            }
            set("bit\t" + std::to_string(y) + "," + mem);
        } else {
            set(std::string(x == 2 ? "res\t" : "set\t") + std::to_string(y) + "," + mem + copy);
        }
    }

    void ed() {
        const std::uint8_t op = next();
        const int x = op >> 6;
        const int y = (op >> 3) & 7;
        const int z = op & 7;
        const int p = y >> 1;
        const int q = y & 1;
        static constexpr const char* kR[8] = {"b", "c", "d", "e", "h", "l", "", "a"};
        if (x == 2 && z <= 3 && y >= 4) {
            set(kBlock[y - 4][z]);
            return;
        }
        if (x != 1) {
            data(2);
            return;
        }
        switch (z) {
            case 0:
                set(y == 6 ? "in\tf,(c)" : "in\t" + std::string(kR[y]) + ",(c)");
                return;
            case 1:
                set(y == 6 ? "out\t(c),0" : "out\t(c)," + std::string(kR[y]));
                return;
            case 2:
                set(std::string(q == 0 ? "sbc\thl," : "adc\thl,") + rp(p));
                return;
            case 3: {
                if (p == 2) {
                    next16();
                    data(4);  // AS emits the shorter unprefixed LD (nn),HL / LD HL,(nn)
                    return;
                }
                const std::string m = "(" + addr(next16()) + ")";
                set(q == 0 ? "ld\t" + m + "," + rp(p) : "ld\t" + rp(p) + "," + m);
                return;
            }
            case 4:
                if (y != 0) {
                    data(2);
                    return;
                }
                set("neg");
                return;
            case 5:
                if (y > 1) {
                    data(2);
                    return;
                }
                set(y == 0 ? "retn" : "reti", FlowType::Return);
                return;
            case 6:
                if (y == 0 || y == 2 || y == 3) {
                    set("im\t" + std::to_string(y == 0 ? 0 : y - 1));
                } else {
                    data(2);
                }
                return;
            default: {
                static constexpr const char* kMisc[6] = {"ld\ti,a", "ld\tr,a", "ld\ta,i",
                                                         "ld\ta,r", "rrd",     "rld"};
                if (y >= 6) {
                    data(2);
                    return;
                }
                set(kMisc[y]);
                return;
            }
        }
    }

    std::span<const std::uint8_t> bytes_;
    std::uint16_t address_;
    const SymbolResolver& symbols_;
    std::size_t pos_ = 0;
    int ix_ = 0;
    bool usesIndex_ = false;
    Instruction ins_;
};

}  // namespace

Instruction disassemble(std::span<const std::uint8_t> bytes, std::uint16_t address,
                        const SymbolResolver& symbols) {
    if (bytes.empty()) {
        Instruction ins;
        ins.address = address;
        ins.length = 0;
        ins.mnemonic = "db";
        return ins;
    }
    return Decoder(bytes, address, symbols).run();
}

}  // namespace gen::analysis::disasmz80
