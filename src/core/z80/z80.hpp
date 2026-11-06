#pragma once
// Zilog Z80 core (spec 03, M04-04 / M04-05). Instruction-atomic: step() runs one whole instruction (or one
// interrupt entry) and returns its T-states. All documented and undocumented opcodes are implemented,
// including the X/Y flags (bits 3 and 5), WZ/MEMPTR, the Q latch that SCF/CCF read, the R register and the
// block-instruction repeat flags. Hand-written from public documentation (Zilog UM0080, "The Undocumented
// Z80 Documented" by Sean Young, the MEMPTR notes and the block-instruction flag research).

#include <cstdint>

#include "core/hooks.hpp"

namespace gen::z80 {

inline constexpr std::uint8_t kFlagC = 0x01;
inline constexpr std::uint8_t kFlagN = 0x02;
inline constexpr std::uint8_t kFlagP = 0x04;  // parity / overflow
inline constexpr std::uint8_t kFlagX = 0x08;  // undocumented bit 3
inline constexpr std::uint8_t kFlagH = 0x10;
inline constexpr std::uint8_t kFlagY = 0x20;  // undocumented bit 5
inline constexpr std::uint8_t kFlagZ = 0x40;
inline constexpr std::uint8_t kFlagS = 0x80;

// Programmer-visible and internal register state (CZ-3).
struct Z80State {
    std::uint8_t a = 0xFF;
    std::uint8_t f = 0xFF;
    std::uint8_t b = 0;
    std::uint8_t c = 0;
    std::uint8_t d = 0;
    std::uint8_t e = 0;
    std::uint8_t h = 0;
    std::uint8_t l = 0;
    std::uint16_t af2 = 0xFFFF;  // shadow registers AF' BC' DE' HL'
    std::uint16_t bc2 = 0;
    std::uint16_t de2 = 0;
    std::uint16_t hl2 = 0;
    std::uint16_t ix = 0xFFFF;
    std::uint16_t iy = 0xFFFF;
    std::uint16_t sp = 0xFFFF;
    std::uint16_t pc = 0;
    std::uint16_t wz = 0;  // MEMPTR
    std::uint8_t i = 0;
    std::uint8_t r = 0;  // bits 0-6 count M1 cycles; bit 7 only changes through LD R,A
    std::uint8_t im = 0;
    bool iff1 = false;
    bool iff2 = false;
    bool halted = false;   // HALT executed: NOPs until an interrupt (PC already points past the HALT)
    bool eiDelay = false;  // the previous instruction was EI: no maskable interrupt before the next one
    bool ldAir = false;    // the previous instruction was LD A,I / LD A,R (interrupt clears P/V; NMOS bug)
    std::uint8_t q = 0;    // F if the previous instruction wrote the flags, else 0 (SCF/CCF X/Y source)
    bool irqLine = false;  // /INT level
    bool nmiPending = false;

    std::uint16_t bc() const { return static_cast<std::uint16_t>((b << 8) | c); }
    std::uint16_t de() const { return static_cast<std::uint16_t>((d << 8) | e); }
    std::uint16_t hl() const { return static_cast<std::uint16_t>((h << 8) | l); }
    std::uint16_t af() const { return static_cast<std::uint16_t>((a << 8) | f); }
    void setBc(std::uint16_t v) {
        b = static_cast<std::uint8_t>(v >> 8);
        c = static_cast<std::uint8_t>(v);
    }
    void setDe(std::uint16_t v) {
        d = static_cast<std::uint8_t>(v >> 8);
        e = static_cast<std::uint8_t>(v);
    }
    void setHl(std::uint16_t v) {
        h = static_cast<std::uint8_t>(v >> 8);
        l = static_cast<std::uint8_t>(v);
    }
    void setAf(std::uint16_t v) {
        a = static_cast<std::uint8_t>(v >> 8);
        f = static_cast<std::uint8_t>(v);
    }
};

// The Z80's view of the system. I/O ports are unconnected on the Genesis (reads return $FF).
class BusZ80 {
public:
    virtual ~BusZ80() = default;
    virtual std::uint8_t read8(std::uint16_t addr) = 0;
    virtual void write8(std::uint16_t addr, std::uint8_t value) = 0;
    // A read without side effects or wait states (the opcode handed to onInstr). Buses whose reads have
    // side effects must override it.
    virtual std::uint8_t peek8(std::uint16_t addr) { return read8(addr); }
    virtual std::uint8_t in8(std::uint16_t port) {
        (void)port;
        return 0xFF;
    }
    virtual void out8(std::uint16_t port, std::uint8_t value) {
        (void)port;
        (void)value;
    }
    // Data bus value during an interrupt acknowledge (IM 0 opcode / IM 2 vector low byte). The Genesis
    // leaves the bus pulled up: $FF (RST 38h in IM 0).
    virtual std::uint8_t irqData() { return 0xFF; }
    // Extra T-states accesses cost (68k bank-window stalls). The bus adds; the CPU charges and clears it.
    int waitCycles = 0;
};

class Z80 {
public:
    explicit Z80(BusZ80& bus);

    // /RESET: PC, I, R, IM and the interrupt flip-flops clear; other registers keep their values.
    void reset();

    // One instruction, interrupt entry or HALT NOP. Returns T-states; 0 when a breakpoint (onInstr) stopped
    // before the instruction.
    int step();

    void setIrq(bool asserted) { s_.irqLine = asserted; }
    void nmi() { s_.nmiPending = true; }

    // The hook table must outlive the CPU (the Machine owns it). Null disables instrumentation.
    void setHooks(const Hooks* hooks) { hooks_ = hooks; }
    bool breakHit() const { return breakHit_; }
    // Address of the executing (or most recently executed) instruction.
    std::uint16_t currentPc() const { return pc0_; }
    // True when the last step() accepted an interrupt (onIrq has been called).
    bool tookInterrupt() const { return tookIrq_; }

    Z80State& state() { return s_; }
    const Z80State& state() const { return s_; }

private:
    // Memory / fetch helpers (all raise onAccess).
    std::uint8_t rd(std::uint16_t addr);
    void wr(std::uint16_t addr, std::uint8_t value);
    std::uint8_t fetchOp();   // M1 cycle: increments R
    std::uint8_t fetchArg();  // operand byte
    std::uint16_t fetchArg16();
    void push(std::uint16_t v);
    std::uint16_t pop();
    void incR() { s_.r = static_cast<std::uint8_t>((s_.r & 0x80) | ((s_.r + 1) & 0x7F)); }
    void setF(std::uint8_t v) {
        s_.f = v;
        flagsWritten_ = true;
    }

    // Register access for the current prefix (mode_: 0 = HL, 1 = IX, 2 = IY). Index 6 is never passed.
    std::uint8_t getReg(int idx) const;
    void setReg(int idx, std::uint8_t v);
    std::uint8_t getRegRaw(int idx) const;  // H / L even with an index prefix
    void setRegRaw(int idx, std::uint8_t v);
    std::uint16_t getHlx() const;  // HL, IX or IY
    void setHlx(std::uint16_t v);
    std::uint16_t getRp(int p) const;  // BC DE HLx SP
    void setRp(int p, std::uint16_t v);
    std::uint16_t memAddrHl();  // (HL) or (IX+d) / (IY+d): fetches d, sets WZ, charges the extra time

    int interrupt();
    int execMain(std::uint8_t op);
    int execCb();
    int execIndexedCb();
    int execEd(std::uint8_t op);
    int blockOp(std::uint8_t op);

    // ALU
    void alu(int op, std::uint8_t v);
    std::uint8_t inc8(std::uint8_t v);
    std::uint8_t dec8(std::uint8_t v);
    std::uint8_t rot(int op, std::uint8_t v);  // CB 00-3F family
    void bit(int n, std::uint8_t v, std::uint8_t xySource);
    std::uint16_t add16(std::uint16_t a, std::uint16_t b);
    void adc16(std::uint16_t v);
    void sbc16(std::uint16_t v);
    void daa();
    bool cond(int cc) const;

    BusZ80& bus_;
    const Hooks* hooks_ = nullptr;
    Z80State s_;
    std::uint16_t pc0_ = 0;
    int mode_ = 0;
    int extra_ = 0;
    bool flagsWritten_ = false;
    bool breakHit_ = false;
    bool tookIrq_ = false;
};

}  // namespace gen::z80
