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
    // An interrupt was accepted at `level` (1-7).
    void (*onIrq)(void* ctx, CpuId cpu, int level) = nullptr;
    void (*onFrameEnd)(void* ctx, std::uint64_t frame) = nullptr;
};

}  // namespace gen
