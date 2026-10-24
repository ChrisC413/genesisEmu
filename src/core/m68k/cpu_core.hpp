#pragma once
// Internal 68000 machinery shared by the instruction handlers. Not part of the public API.
//
// Timing model: every bus access adds its cycles (4 per word, 8 per long) at the moment it happens and
// internal delays are added with idle(). Because the order matches the real microcode closely enough, an
// address error both stacks the right PC and costs the right number of cycles (cycles so far + 50).
//
// Prefetch model: during execution `s.pc` is the address of the word in IRC. fetchExt() consumes IRC and
// refills it from the next word; prefetch() moves IRC into IR at the end of an instruction. State::pc (the
// address of IR) is restored at the end of step().

#include <array>
#include <cstdint>
#include <memory>

#include "core/hooks.hpp"
#include "core/m68k/m68k.hpp"

namespace gen::m68k {

struct CpuCore;
using Handler = void (*)(CpuCore& c, std::uint32_t op);

struct DecodeTable {
    std::array<Handler, 65536> handlers{};
    std::array<bool, 65536> valid{};
};

const DecodeTable& decodeTable();
std::unique_ptr<DecodeTable> buildDecodeTable();

// Thrown by an odd word/long access; caught by step() which builds the group 0 frame.
struct AddressErrorTrap {
    std::uint32_t addr;
    bool read;
    bool program;  // instruction stream access
};

// Out of line so the bus primitives stay small enough to inline.
[[noreturn]] void throwAddressError(std::uint32_t addr, bool read, bool program);

// Effective address modes as decoded from the 6-bit mode/register field.
enum Mode : int {
    DReg,     // Dn
    AReg,     // An
    Ind,      // (An)
    PostInc,  // (An)+
    PreDec,   // -(An)
    Disp,     // d16(An)
    Index,    // d8(An,Xn)
    AbsW,     // abs.W
    AbsL,     // abs.L
    PcDisp,   // d16(PC)
    PcIndex,  // d8(PC,Xn)
    Imm,      // #imm
    Invalid,
};

constexpr Mode decodeMode(unsigned mode, unsigned reg) {
    if (mode < 7) {
        return static_cast<Mode>(mode);
    }
    switch (reg) {
        case 0:
            return AbsW;
        case 1:
            return AbsL;
        case 2:
            return PcDisp;
        case 3:
            return PcIndex;
        case 4:
            return Imm;
        default:
            return Invalid;
    }
}
constexpr Mode eaMode(std::uint32_t op) {
    return decodeMode((op >> 3) & 7, op & 7);
}

// Addressing categories (68000 PRM table 2-4).
constexpr bool isData(Mode m) {
    return m != AReg && m != Invalid;
}
constexpr bool isMemory(Mode m) {
    return m != DReg && m != AReg && m != Invalid;
}
constexpr bool isAlterable(Mode m) {
    return m != PcDisp && m != PcIndex && m != Imm && m != Invalid;
}
constexpr bool isDataAlterable(Mode m) {
    return isData(m) && isAlterable(m);
}
constexpr bool isMemoryAlterable(Mode m) {
    return isMemory(m) && isAlterable(m);
}
constexpr bool isControl(Mode m) {
    return m == Ind || m == Disp || m == Index || m == AbsW || m == AbsL || m == PcDisp || m == PcIndex;
}

template <int S>
constexpr std::uint32_t sizeMask() {
    return S == 1 ? 0xFFu : S == 2 ? 0xFFFFu : 0xFFFFFFFFu;
}
template <int S>
constexpr std::uint32_t signBit() {
    return S == 1 ? 0x80u : S == 2 ? 0x8000u : 0x80000000u;
}
template <int S>
constexpr std::int32_t signExtend(std::uint32_t v) {
    if constexpr (S == 1) {
        return static_cast<std::int8_t>(v);
    } else if constexpr (S == 2) {
        return static_cast<std::int16_t>(v);
    } else {
        return static_cast<std::int32_t>(v);
    }
}

struct CpuCore {
    State s;
    Bus68k* bus = nullptr;
    Hooks hooks{};
    const DecodeTable* table = nullptr;
    int cycles = 0;             // cycles of the current step so far
    std::uint32_t instrPc = 0;  // address of the executing instruction
    int irqLevel = 0;           // IPL input
    bool nmiPending = false;    // level 7 is edge-triggered
    bool tasWriteback = false;
    bool skipBreak = false;
    bool broke = false;
    Flow flow = Flow::Sequential;

    // ---- Bus primitives
    // -------------------------------------------------------------------------------------

    void access(AccessKind kind, std::uint32_t addr, std::uint8_t size, std::uint32_t value) const {
        if (hooks.onAccess != nullptr) {
            hooks.onAccess(hooks.ctx, CpuId::M68k, kind, addr, size, value, instrPc);
        }
    }
    std::uint16_t readWord(std::uint32_t addr, bool program = false) {
        if ((addr & 1) != 0) {
            throwAddressError(addr, true, program);
        }
        cycles += 4;
        const std::uint16_t v = bus->read16(addr & 0xFFFFFF);
        access(program ? AccessKind::Fetch : AccessKind::Read, addr & 0xFFFFFF, 2, v);
        return v;
    }
    std::uint8_t readByte(std::uint32_t addr) {
        cycles += 4;
        const std::uint8_t v = bus->read8(addr & 0xFFFFFF);
        access(AccessKind::Read, addr & 0xFFFFFF, 1, v);
        return v;
    }
    void writeWord(std::uint32_t addr, std::uint16_t v) {
        if ((addr & 1) != 0) {
            throwAddressError(addr, false, false);
        }
        cycles += 4;
        access(AccessKind::Write, addr & 0xFFFFFF, 2, v);
        bus->write16(addr & 0xFFFFFF, v);
    }
    void writeByte(std::uint32_t addr, std::uint8_t v) {
        cycles += 4;
        access(AccessKind::Write, addr & 0xFFFFFF, 1, v);
        bus->write8(addr & 0xFFFFFF, v);
    }

    template <int S>
    std::uint32_t read(std::uint32_t addr) {
        if constexpr (S == 1) {
            return readByte(addr);
        } else if constexpr (S == 2) {
            return readWord(addr);
        } else {
            const std::uint32_t hi = readWord(addr);
            return (hi << 16) | readWord(addr + 2);
        }
    }
    template <int S>
    void write(std::uint32_t addr, std::uint32_t v) {
        if constexpr (S == 1) {
            writeByte(addr, static_cast<std::uint8_t>(v));
        } else if constexpr (S == 2) {
            writeWord(addr, static_cast<std::uint16_t>(v));
        } else {
            writeWord(addr, static_cast<std::uint16_t>(v >> 16));
            writeWord(addr + 2, static_cast<std::uint16_t>(v));
        }
    }
    // Long write in the order -(An) destinations use: low word first.
    void writeLongDescending(std::uint32_t addr, std::uint32_t v) {
        writeWord(addr + 2, static_cast<std::uint16_t>(v));
        writeWord(addr, static_cast<std::uint16_t>(v >> 16));
    }
    // -(An) long write that decrements An by 2 before each word (an address error leaves An 2 lower).
    void writePreDecLong(unsigned reg, std::uint32_t v) {
        s.a[reg] -= 2;
        writeWord(s.a[reg], static_cast<std::uint16_t>(v));
        s.a[reg] -= 2;
        writeWord(s.a[reg], static_cast<std::uint16_t>(v >> 16));
    }

    void idle(int n) { cycles += n; }

    // ---- Prefetch
    // -------------------------------------------------------------------------------------------

    // Consumes the extension word in IRC and refills IRC from the following word.
    std::uint16_t fetchExt() {
        const std::uint16_t w = s.irc;
        s.pc += 2;
        s.irc = readWord(s.pc, true);
        return w;
    }
    std::uint32_t fetchExtLong() {
        const std::uint32_t hi = fetchExt();
        return (hi << 16) | fetchExt();
    }
    // End-of-instruction prefetch: IRC moves to IR and the next word is fetched.
    void prefetch() {
        s.ir = s.irc;
        s.pc += 2;
        s.irc = readWord(s.pc, true);
    }
    // Loads the queue at a new program counter (two fetches).
    void jump(std::uint32_t target) {
        s.pc = target - 2;  // an odd target faults on the first fetch, which stacks target - 4
        s.ir = readWord(target, true);
        s.irc = readWord(target + 2, true);
        s.pc = target + 2;
    }

    // ---- Status register
    // ------------------------------------------------------------------------------------

    void setSr(std::uint16_t v) {
        v &= kSrValidBits;
        if (((v ^ s.sr) & kSrSupervisor) != 0) {
            if ((v & kSrSupervisor) != 0) {
                s.usp = s.a[7];
                s.a[7] = s.ssp;
            } else {
                s.ssp = s.a[7];
                s.a[7] = s.usp;
            }
        }
        s.sr = v;
    }
    void setCcr(std::uint32_t v) { s.sr = static_cast<std::uint16_t>((s.sr & 0xFF00) | (v & 0x1F)); }
    bool flag(std::uint16_t f) const { return (s.sr & f) != 0; }
    void setFlag(std::uint16_t f, bool on) {
        s.sr = static_cast<std::uint16_t>(on ? (s.sr | f) : (s.sr & ~f));
    }

    template <int S>
    void setNZ(std::uint32_t r) {
        r &= sizeMask<S>();
        s.sr = static_cast<std::uint16_t>((s.sr & ~(kSrNegative | kSrZero)) | (r == 0 ? kSrZero : 0) |
                                          ((r & signBit<S>()) != 0 ? kSrNegative : 0));
    }
    // N and Z from the result, V and C cleared, X unchanged (MOVE, logic ops, TST, ...).
    template <int S>
    void setLogicFlags(std::uint32_t r) {
        s.sr = static_cast<std::uint16_t>(s.sr & ~(kSrOverflow | kSrCarry));
        setNZ<S>(r);
    }

    // Condition codes (PRM table 3-19).
    bool testCondition(unsigned cc) const {
        const bool c = flag(kSrCarry);
        const bool v = flag(kSrOverflow);
        const bool z = flag(kSrZero);
        const bool n = flag(kSrNegative);
        switch (cc & 15) {
            case 0:
                return true;  // T
            case 1:
                return false;  // F
            case 2:
                return !c && !z;  // HI
            case 3:
                return c || z;  // LS
            case 4:
                return !c;  // CC
            case 5:
                return c;  // CS
            case 6:
                return !z;  // NE
            case 7:
                return z;  // EQ
            case 8:
                return !v;  // VC
            case 9:
                return v;  // VS
            case 10:
                return !n;  // PL
            case 11:
                return n;  // MI
            case 12:
                return n == v;  // GE
            case 13:
                return n != v;  // LT
            case 14:
                return !z && n == v;  // GT
            default:
                return z || n != v;  // LE
        }
    }

    // ---- Stack
    // ----------------------------------------------------------------------------------------------

    void push16(std::uint16_t v) {
        s.a[7] -= 2;
        writeWord(s.a[7], v);
    }
    void push32(std::uint32_t v) {
        s.a[7] -= 4;
        write<4>(s.a[7], v);
    }
    std::uint16_t pop16() {
        const std::uint16_t v = readWord(s.a[7]);
        s.a[7] += 2;
        return v;
    }
    std::uint32_t pop32() {
        const std::uint32_t v = read<4>(s.a[7]);
        s.a[7] += 4;
        return v;
    }

    // ---- Exceptions
    // -----------------------------------------------------------------------------------------

    // Group 1/2 exception: stacks `stackedPc` and SR, enters supervisor mode, loads the vector and refills
    // the prefetch queue. `idleCycles` is the internal time before the first stack write.
    void exception(int vector, std::uint32_t stackedPc, int idleCycles);
    void privilegeViolation();
    void illegalInstruction(int vector);

    // ---- Effective addresses
    // --------------------------------------------------------------------------------

    std::uint32_t& dreg(std::uint32_t op) { return s.d[op & 7]; }
    std::uint32_t& areg(std::uint32_t op) { return s.a[op & 7]; }

    // d8(base, Xn) using the brief extension word in IRC (68000: scale and full formats are ignored).
    std::uint32_t indexed(std::uint32_t base, std::uint16_t ext) const {
        const std::uint32_t xn = (ext & 0x8000) != 0 ? s.a[(ext >> 12) & 7] : s.d[(ext >> 12) & 7];
        const std::int32_t index =
            (ext & 0x0800) != 0 ? static_cast<std::int32_t>(xn) : static_cast<std::int16_t>(xn & 0xFFFF);
        return base + static_cast<std::uint32_t>(static_cast<std::int8_t>(ext & 0xFF)) +
               static_cast<std::uint32_t>(index);
    }

    // Computes a memory operand address with all register side effects, extension word fetches and the
    // internal EA time (2 cycles for -(An) and indexed modes).
    template <int S, Mode M>
    std::uint32_t eaAddress(unsigned reg) {
        constexpr std::uint32_t step = S == 4 ? 4 : 2;
        if constexpr (M == Ind) {
            return s.a[reg];
        } else if constexpr (M == PostInc) {
            const std::uint32_t a = s.a[reg];
            s.a[reg] += (S == 1 && reg != 7) ? 1 : step;
            return a;
        } else if constexpr (M == PreDec) {
            idle(2);
            s.a[reg] -= (S == 1 && reg != 7) ? 1 : step;
            return s.a[reg];
        } else if constexpr (M == Disp) {
            const std::uint32_t base = s.a[reg];
            return base + static_cast<std::uint32_t>(static_cast<std::int16_t>(fetchExt()));
        } else if constexpr (M == Index) {
            idle(2);
            const std::uint32_t base = s.a[reg];
            return indexed(base, fetchExt());
        } else if constexpr (M == AbsW) {
            return static_cast<std::uint32_t>(static_cast<std::int16_t>(fetchExt()));
        } else if constexpr (M == AbsL) {
            return fetchExtLong();
        } else if constexpr (M == PcDisp) {
            const std::uint32_t base = s.pc;
            return base + static_cast<std::uint32_t>(static_cast<std::int16_t>(fetchExt()));
        } else if constexpr (M == PcIndex) {
            idle(2);
            const std::uint32_t base = s.pc;
            return indexed(base, fetchExt());
        } else {
            static_assert(M == Ind, "not a memory mode");
            return 0;
        }
    }

    template <int S>
    std::uint32_t immediate() {
        if constexpr (S == 4) {
            return fetchExtLong();
        } else if constexpr (S == 2) {
            return fetchExt();
        } else {
            return fetchExt() & 0xFFu;
        }
    }

    // Reads a source operand of size S.
    template <int S, Mode M>
    std::uint32_t readEa(unsigned reg) {
        if constexpr (M == DReg) {
            return s.d[reg] & sizeMask<S>();
        } else if constexpr (M == AReg) {
            return s.a[reg] & sizeMask<S>();
        } else if constexpr (M == Imm) {
            return immediate<S>();
        } else {
            return read<S>(eaAddress<S, M>(reg));
        }
    }

    template <int S>
    void writeDreg(unsigned reg, std::uint32_t v) {
        s.d[reg] = (s.d[reg] & ~sizeMask<S>()) | (v & sizeMask<S>());
    }
};

// Mode -> handler dispatch for templated handlers: byMode(m, []<Mode M>() { return &handler<M>; }).
template <typename F>
Handler byMode(Mode m, F&& f) {
    switch (m) {
        case DReg:
            return f.template operator()<DReg>();
        case AReg:
            return f.template operator()<AReg>();
        case Ind:
            return f.template operator()<Ind>();
        case PostInc:
            return f.template operator()<PostInc>();
        case PreDec:
            return f.template operator()<PreDec>();
        case Disp:
            return f.template operator()<Disp>();
        case Index:
            return f.template operator()<Index>();
        case AbsW:
            return f.template operator()<AbsW>();
        case AbsL:
            return f.template operator()<AbsL>();
        case PcDisp:
            return f.template operator()<PcDisp>();
        case PcIndex:
            return f.template operator()<PcIndex>();
        case Imm:
            return f.template operator()<Imm>();
        default:
            return nullptr;
    }
}

// Table construction helpers.
struct TableBuilder {
    DecodeTable& t;
    // Calls fn(op) for every opcode with (op & mask) == match.
    template <typename F>
    void forEach(std::uint16_t mask, std::uint16_t match, F&& fn) {
        for (std::uint32_t op = 0; op < 65536; ++op) {
            if ((op & mask) == match) {
                fn(op);
            }
        }
    }
    void set(std::uint32_t op, Handler h) {
        if (h != nullptr) {
            t.handlers[op] = h;
            t.valid[op] = true;
        }
    }
};

void registerDataMovement(TableBuilder& b);
void registerArithmetic(TableBuilder& b);
void registerLogic(TableBuilder& b);
void registerShiftBit(TableBuilder& b);
void registerBcd(TableBuilder& b);
void registerProgramControl(TableBuilder& b);
void registerSystem(TableBuilder& b);

}  // namespace gen::m68k
