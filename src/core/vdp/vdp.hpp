#pragma once
// VDP port stub (M02-04, spec 05 "Ports" / "Interrupts and timing"): command latch, register writes,
// VRAM / CRAM / VSRAM storage, status register, HV counter, V-int / H-int, and instant DMA. No rendering
// yet (M03). This grows into the full VDP in M03-01.

#include <array>
#include <cstdint>

#include "core/hooks.hpp"

namespace gen::vdp {

enum class DmaKind : std::uint8_t { MemoryToVdp, Fill, Copy };
enum class DmaTarget : std::uint8_t { Vram, Cram, Vsram };

inline constexpr std::size_t kVramSize = 0x10000;
inline constexpr std::size_t kCramWords = 64;
inline constexpr std::size_t kVsramWords = 40;
inline constexpr int kRegisterCount = 24;

struct VdpState {
    std::array<std::uint8_t, kRegisterCount> regs{};
    std::array<std::uint8_t, kVramSize> vram{};
    std::array<std::uint16_t, kCramWords> cram{};
    std::array<std::uint16_t, kVsramWords> vsram{};
    std::uint16_t address = 0;  // 16-bit access address (A15-A0)
    std::uint8_t code = 0;      // CD5-CD0: access target, DMA bit
    bool pending = false;       // first command word latched, waiting for the second
    bool fillPending = false;   // DMA fill armed: the next data-port write starts it
    bool vintPending = false;   // status F flag
    bool hintPending = false;
    bool spriteOverflow = false;
    bool spriteCollision = false;
    bool oddFrame = false;
    bool pal = false;
    int hintCounter = 0;
    int line = 0;  // current scanline (0 = first active line)
};

}  // namespace gen::vdp

namespace gen {

// onDma hook payload (spec 01 / 05).
struct DmaEvent {
    vdp::DmaKind kind = vdp::DmaKind::MemoryToVdp;
    vdp::DmaTarget target = vdp::DmaTarget::Vram;
    std::uint32_t source = 0;  // 68k byte address (MemoryToVdp) or VRAM byte address (Copy); 0 for Fill
    std::uint32_t dest = 0;    // byte address in the target memory
    std::uint32_t length = 0;  // words (MemoryToVdp) or bytes (Fill, Copy)
};

}  // namespace gen

namespace gen::vdp {

class Vdp {
public:
    // Reads one word of 68k memory for DMA (ROM / RAM, no side effects).
    using DmaReadFn = std::uint16_t (*)(void* ctx, std::uint32_t addr);

    void reset(bool pal);
    void setDmaReader(DmaReadFn fn, void* ctx) {
        dmaRead_ = fn;
        dmaCtx_ = ctx;
    }
    void setHooks(const Hooks* hooks) { hooks_ = hooks; }

    // Ports. `lineMclk` is the master-clock position inside the current scanline (0-3419).
    std::uint16_t readData();
    void writeData(std::uint16_t value);
    std::uint16_t readStatus(int lineMclk);
    void writeControl(std::uint16_t value);
    std::uint16_t readHvCounter(int lineMclk) const;

    // Scheduler: scanline `line` starts (0 = first active line). Updates V-blank, the H-int counter and
    // raises V-int at the first V-blank line.
    void beginLine(int line);

    // Interrupt level the VDP drives on the 68k IPL lines (6, 4 or 0).
    int irqLevel() const;
    void acknowledge(int level);

    int activeHeight() const { return (s_.regs[1] & 0x08) != 0 ? 240 : 224; }
    bool h40() const { return (s_.regs[12] & 0x01) != 0; }
    bool inVblank() const { return s_.line >= activeHeight() || (s_.regs[1] & 0x40) == 0; }

    VdpState& state() { return s_; }
    const VdpState& state() const { return s_; }

private:
    int autoIncrement() const { return s_.regs[15]; }
    std::uint32_t dmaLength() const;
    void writeTarget(std::uint16_t value);
    void startDma();
    void runFill(std::uint16_t value);
    std::uint8_t vCounter() const;
    std::uint8_t hCounter(int lineMclk) const;

    VdpState s_;
    DmaReadFn dmaRead_ = nullptr;
    void* dmaCtx_ = nullptr;
    const Hooks* hooks_ = nullptr;
};

}  // namespace gen::vdp
