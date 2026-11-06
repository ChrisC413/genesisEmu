#pragma once
// Disassembly listing for the live view (M06-10). Disassembling forward is exact; going backwards from an
// address is ambiguous on both CPUs (variable-length instructions), so previousStart() picks the candidate
// that is a known instruction start (a PC that executed: the current PC, the ring buffer, the call stack,
// earlier stops) or, failing that, the one with the longest chain of valid instructions before it.

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_set>
#include <vector>

#include "core/hooks.hpp"

namespace gen {
class Machine;
}

namespace gen::debug {

struct ListingLine {
    std::uint32_t addr = 0;
    std::uint8_t length = 1;
    std::string bytes;  // hex
    std::string text;   // "mnemonic operands"
    bool valid = true;
};

// Addresses known to start an instruction.
class KnownStarts {
public:
    void add(CpuId cpu, std::uint32_t addr) { set(cpu).insert(addr); }
    bool contains(CpuId cpu, std::uint32_t addr) const {
        const auto& s = cpu == CpuId::Z80 ? z80_ : m68k_;
        return s.contains(addr);
    }
    void clear() {
        m68k_.clear();
        z80_.clear();
    }
    std::size_t size() const { return m68k_.size() + z80_.size(); }

private:
    std::unordered_set<std::uint32_t>& set(CpuId cpu) { return cpu == CpuId::Z80 ? z80_ : m68k_; }
    std::unordered_set<std::uint32_t> m68k_;
    std::unordered_set<std::uint32_t> z80_;
};

ListingLine disassembleAt(const Machine& m, CpuId cpu, std::uint32_t addr);
// The start of the instruction before `addr`.
std::uint32_t previousStart(const Machine& m, CpuId cpu, std::uint32_t addr, const KnownStarts& known);
// `count` lines starting `before` instructions above `anchor` (so the anchor is line `before` when the
// backward walk does not hit address 0).
std::vector<ListingLine> buildListing(const Machine& m, CpuId cpu, std::uint32_t anchor, int before,
                                      int count, const KnownStarts& known);

}  // namespace gen::debug
