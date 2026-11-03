#pragma once
// Instrumentation hook API (spec 01). Function pointers + context: no std::function and no virtual dispatch
// on the hot path. When a pointer is null the cost is one predictable branch.

#include <cstdint>

namespace gen {

enum class CpuId : std::uint8_t { M68k, Z80 };

enum class AccessKind : std::uint8_t {
    Fetch,  // instruction stream (opcode, extension words, prefetch)
    Read,
    Write,
};

enum class Action : std::uint8_t { Continue, Break };

// What a VDP write reached (onVdpWrite).
enum class VdpWriteKind : std::uint8_t { Register, Vram, Cram, Vsram };

struct DmaEvent;  // defined with the VDP (M03)

struct Hooks {
    void* ctx = nullptr;
    // Before each instruction executes. Returning Action::Break stops before the instruction runs.
    Action (*onInstr)(void* ctx, CpuId cpu, std::uint32_t pc, std::uint16_t opcode) = nullptr;
    // Every CPU memory access, after address decode and before side effects. `size` is 1, 2 or 4 bytes;
    // `value` is the value read or written. `pc` is the address of the instruction making the access.
    void (*onAccess)(void* ctx, CpuId cpu, AccessKind kind, std::uint32_t addr, std::uint8_t size,
                     std::uint32_t value, std::uint32_t pc) = nullptr;
    void (*onDma)(void* ctx, const DmaEvent& event) = nullptr;
    // A Z80 access through the 68k bank window ($8000-$FFFF), with the translated 68k address (CZ-6). The
    // Z80-local access is also reported through onAccess.
    void (*onZ80Window)(void* ctx, AccessKind kind, std::uint16_t z80Addr, std::uint32_t m68kAddr,
                        std::uint8_t value, std::uint16_t pc) = nullptr;
    // An interrupt was accepted at `level` (1-7).
    void (*onIrq)(void* ctx, CpuId cpu, int level) = nullptr;
    // The 68k entered an exception, trap or interrupt (`vector`, e.g. 30 = V-int); raised by the scheduler
    // after the entry, so the PC is the handler's first instruction. `pc` is the instruction (or the
    // interrupted PC) that caused it.
    void (*onException)(void* ctx, CpuId cpu, int vector, std::uint32_t pc) = nullptr;
    // A VDP register write (`addr` = register number) or a write to VRAM / CRAM / VSRAM through the data
    // port or a 68k -> VDP DMA (`addr` = byte address in that memory), before it is stored. Fill and copy
    // DMAs are reported through onDma only.
    void (*onVdpWrite)(void* ctx, VdpWriteKind kind, std::uint32_t addr, std::uint16_t value) = nullptr;
    void (*onFrameEnd)(void* ctx, std::uint64_t frame) = nullptr;
};

}  // namespace gen
