#pragma once
// Side-effect-free memory access for the debugger (M06-06): every memory the hex viewers show, reads that
// never touch I/O or VDP ports, edits, ROM patches and byte-pattern search.

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "core/hooks.hpp"

namespace gen {
class Machine;
}

namespace gen::debug {

// A byte of a CPU's address space, if it is plain memory (ROM, RAM, Z80 RAM); nullopt for I/O and ports.
std::optional<std::uint8_t> peek(const Machine& m, CpuId cpu, std::uint32_t addr);
// Big-endian (68k) / little-endian (Z80) value of `size` bytes; unreadable bytes read as 0.
std::uint32_t peekValue(const Machine& m, CpuId cpu, std::uint32_t addr, int size);

enum class Region : std::uint8_t { Rom, Ram68k, RamZ80, Vram, Cram, Vsram, Sram, Count };

struct RegionInfo {
    const char* name;
    std::uint32_t displayBase;  // address shown for offset 0 (e.g. $FF0000 for 68k RAM)
};
RegionInfo regionInfo(Region region);
std::size_t regionSize(const Machine& m, Region region);
// CRAM and VSRAM are shown as big-endian bytes of their words.
std::optional<std::uint8_t> readRegion(const Machine& m, Region region, std::size_t offset);

struct RomPatch {
    std::uint32_t addr = 0;
    std::uint8_t original = 0;
    std::uint8_t value = 0;
};

// Writes one byte (call while paused). ROM writes are recorded in `patches` (an edit back to the original
// value removes the patch). Returns false outside the region.
bool writeRegion(Machine& m, Region region, std::size_t offset, std::uint8_t value,
                 std::vector<RomPatch>& patches);

// A search pattern: hex bytes, `??` for any byte, optionally separated by spaces ("4E 75", "4e75", "60??4e").
struct BytePattern {
    std::vector<std::uint8_t> bytes;
    std::vector<bool> wildcard;
};
std::optional<BytePattern> parsePattern(std::string_view text, std::string& error);
// Offsets of every (possibly overlapping) occurrence in `data`.
std::vector<std::size_t> findAll(std::span<const std::uint8_t> data, const BytePattern& pattern,
                                 std::size_t limit = 100000);
std::vector<std::size_t> findAll(const Machine& m, Region region, const BytePattern& pattern,
                                 std::size_t limit = 100000);

// A copy of a region (for "changed since the last stop" highlighting).
std::vector<std::uint8_t> snapshot(const Machine& m, Region region);

}  // namespace gen::debug
