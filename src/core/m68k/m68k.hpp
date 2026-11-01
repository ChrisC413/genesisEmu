#pragma once
// Motorola 68000 core (spec 02). Instruction-accurate: step() runs one whole instruction (or one exception /
// interrupt entry) and returns its cycle count. The two-word prefetch queue (IR/IRC) is modeled at the
// register level; bus-cycle ordering inside an instruction is only modeled as far as it changes visible
// state (cycle counts and the stacked frame of an address error).

#include <array>
#include <cstdint>
#include <memory>

#include "core/hooks.hpp"

namespace gen::m68k {

inline constexpr std::uint16_t kSrCarry = 0x0001;
inline constexpr std::uint16_t kSrOverflow = 0x0002;
inline constexpr std::uint16_t kSrZero = 0x0004;
inline constexpr std::uint16_t kSrNegative = 0x0008;
inline constexpr std::uint16_t kSrExtend = 0x0010;
inline constexpr std::uint16_t kSrIntMask = 0x0700;
inline constexpr std::uint16_t kSrSupervisor = 0x2000;
inline constexpr std::uint16_t kSrTrace = 0x8000;
inline constexpr std::uint16_t kSrValidBits = 0xA71F;  // T, S, I2-I0, X, N, Z, V, C

// Full programmer-visible register state (C68-7). A7 is the active stack pointer; the inactive one lives in
// `usp` or `ssp`. Use the accessors to read either stack pointer regardless of the current mode.
struct State {
    std::array<std::uint32_t, 8> d{};
    std::array<std::uint32_t, 8> a{};
    std::uint32_t usp = 0;  // valid while in supervisor mode (otherwise A7 holds it)
    std::uint32_t ssp = 0;  // valid while in user mode (otherwise A7 holds it)
    std::uint32_t pc = 0;   // address of the instruction in IR (the next one to execute)
    std::uint16_t sr = kSrSupervisor | kSrIntMask;
    std::uint16_t ir = 0;   // prefetch queue: the opcode at pc
    std::uint16_t irc = 0;  // prefetch queue: the word at pc + 2
    bool stopped = false;   // STOP executed; waiting for an interrupt
    bool halted = false;    // double bus fault; only reset recovers

    bool supervisor() const { return (sr & kSrSupervisor) != 0; }
    std::uint32_t userSp() const { return supervisor() ? usp : a[7]; }
    std::uint32_t supervisorSp() const { return supervisor() ? a[7] : ssp; }
    void setUserSp(std::uint32_t v) { (supervisor() ? usp : a[7]) = v; }
    void setSupervisorSp(std::uint32_t v) { (supervisor() ? a[7] : ssp) = v; }
};

// The CPU's view of the system bus. Addresses are 24-bit (already masked). Word accesses are always even.
class Bus68k {
public:
    virtual ~Bus68k() = default;
    virtual std::uint8_t read8(std::uint32_t addr) = 0;
    virtual std::uint16_t read16(std::uint32_t addr) = 0;
    virtual void write8(std::uint32_t addr, std::uint8_t value) = 0;
    virtual void write16(std::uint32_t addr, std::uint16_t value) = 0;
    virtual std::uint32_t read32(std::uint32_t addr) {
        return (static_cast<std::uint32_t>(read16(addr)) << 16) | read16((addr + 2) & 0xFFFFFF);
    }
    virtual void write32(std::uint32_t addr, std::uint32_t value) {
        write16(addr, static_cast<std::uint16_t>(value >> 16));
        write16((addr + 2) & 0xFFFFFF, static_cast<std::uint16_t>(value));
    }
    // Interrupt acknowledge for `level`; returns a vector number, or kAutovector / kSpurious.
    virtual int iack(int level) {
        (void)level;
        return kAutovector;
    }
    // The RESET instruction pulsed the external reset line.
    virtual void resetDevices() {}
    // Wait-state cycles accesses cost (e.g. Z80-area access). The bus adds to this counter; the CPU adds it
    // to the instruction time and clears it after each step.
    int waitCycles = 0;

    // Optional direct-access pages: 256 x 64 KB. When fastRead[addr >> 16] is set the CPU reads plain memory
    // at that pointer (offset addr & $FFFF) instead of calling read8/read16; fastWrite likewise for writes.
    // Only for side-effect-free memory (ROM, work RAM). Hooks still see every access.
    std::array<const std::uint8_t*, 256> fastRead{};
    std::array<std::uint8_t*, 256> fastWrite{};

    static constexpr int kAutovector = -1;
    static constexpr int kSpurious = -2;
};

// What the last step did to control flow (used by the debugger's step-over and by coverage).
enum class Flow : std::uint8_t {
    Sequential,  // fell through to the next instruction (including a not-taken branch)
    Branch,      // taken Bcc/BRA/DBcc
    Jump,        // JMP
    Call,        // BSR/JSR
    Return,      // RTS/RTR/RTE
    Exception,   // trap, interrupt, reset or fault: entered a handler
    Stopped,     // idle while STOPped or halted
};

struct DecodeTable;
struct CpuCore;

class M68k {
public:
    explicit M68k(Bus68k& bus);
    ~M68k();
    M68k(const M68k&) = delete;
    M68k& operator=(const M68k&) = delete;

    // Reset exception: supervisor mode, mask 7, SSP from $000000 and PC from $000004.
    int reset();

    // Executes one instruction, or takes a pending interrupt / exception, and returns the cycles used. If the
    // onInstr hook asks to break, nothing executes, 0 is returned and breakHit() is true; the next step()
    // then runs that instruction without consulting the hook again.
    int step();
    bool breakHit() const;

    // Interrupt priority level input (0 = none, 7 = NMI). Sampled between instructions.
    void setIrqLevel(int level);
    int irqLevel() const;
    // Save states: the latched level-7 edge and the IPL input, restored without edge detection.
    bool nmiPending() const;
    void restoreIrq(int level, bool nmiPending);

    State& state();
    const State& state() const;
    void setHooks(const Hooks& hooks);

    // C68-9: on the Genesis the TAS write cycle does not reach cartridge/RAM. Default: write-back ignored.
    void setTasWriteback(bool enabled);

    Flow lastFlow() const;
    // Vector number of the exception, trap or interrupt the last step entered (e.g. 3 = address error,
    // 30 = level-6 autovector), or -1 when it entered none. Reset is not reported.
    int lastException() const;
    // Address of the instruction being executed; valid inside bus callbacks during step().
    std::uint32_t currentPc() const;

    // Re-fills IR/IRC from memory at state().pc (after editing PC from a debugger or test).
    void refillPrefetch();

private:
    std::unique_ptr<CpuCore> core_;
};

// Builds (and discards) a fresh 65,536-entry decode table; returns the number of valid opcodes. The shared
// table is built once on first use; this exists for the construction-time test.
int buildDecodeTableOnce();
// True if the core decodes `opcode` as a valid 68000 instruction (not the illegal / line-A / line-F path).
bool isValidOpcode(std::uint16_t opcode);

}  // namespace gen::m68k
