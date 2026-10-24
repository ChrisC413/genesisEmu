#pragma once
// Text instruction trace (spec 08 "Tracing", M02-06). One line per 68k instruction or interrupt entry:
//
//   <frame> <line> <mclk> M68K <pc> <opcode words> <disassembly> | <register deltas> | <memory writes>
//
// The exact format is documented in docs/specs/08-debugger.md. Attaching the writer installs Machine
// hooks; with no writer attached the core pays nothing beyond its null-pointer checks.

#include <array>
#include <cstdint>
#include <ostream>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "core/m68k/m68k.hpp"

namespace gen {
class Machine;
}

namespace gen::instrument {

struct TraceConfig {
    std::uint64_t startFrame = 0;                     // first frame logged
    std::uint64_t endFrame = UINT64_MAX;              // first frame not logged
    std::optional<std::pair<std::uint32_t, std::uint32_t>> pcRange;  // inclusive
    bool firstExecutionOnly = false;                  // log each PC once
    std::uint64_t maxRecords = UINT64_MAX;
};

// Disassembles the instruction at `addr` from `bytes`; returns the text and sets `length` (bytes).
using DisassembleFn = std::function<std::string(std::span<const std::uint8_t> bytes, std::uint32_t addr,
                                                int& length)>;

class TraceWriter {
public:
    // `out` stays owned by the caller. Text is buffered and written in large blocks.
    TraceWriter(Machine& machine, std::ostream* out, TraceConfig config, DisassembleFn disassemble);
    ~TraceWriter();
    TraceWriter(const TraceWriter&) = delete;
    TraceWriter& operator=(const TraceWriter&) = delete;

    // Installs / removes the hooks. finish() completes the last record and flushes.
    void attach();
    void finish();

    std::uint64_t records() const { return records_; }

    static constexpr const char* kHeader = "# genesisEmu trace v1";

private:
    struct Write {
        std::uint32_t addr;
        std::uint32_t value;
        std::uint8_t size;
    };
    struct Regs {
        std::array<std::uint32_t, 8> d{};
        std::array<std::uint32_t, 8> a{};
        std::uint32_t otherSp = 0;  // the inactive stack pointer
        std::uint16_t sr = 0;
    };

    static Action onInstr(void* ctx, CpuId cpu, std::uint32_t pc, std::uint16_t opcode);
    static void onAccess(void* ctx, CpuId cpu, AccessKind kind, std::uint32_t addr, std::uint8_t size,
                         std::uint32_t value, std::uint32_t pc);
    static void onIrq(void* ctx, CpuId cpu, int level);

    Regs snapshot() const;
    bool frameInRange() const;
    void begin(std::uint32_t pc, int irqLevel);
    void complete();
    void flush();

    Machine& machine_;
    std::ostream* out_;
    TraceConfig config_;
    DisassembleFn disassemble_;
    std::vector<std::uint8_t> seen_;  // firstExecutionOnly: one bit per 24-bit PC
    std::string buffer_;
    bool open_ = false;  // a record is waiting for its deltas
    std::string line_;   // the open record's text up to the disassembly
    Regs before_;
    std::vector<Write> writes_;
    std::uint64_t records_ = 0;
};

}  // namespace gen::instrument
