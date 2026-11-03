#include "debug/listing.hpp"

#include <algorithm>
#include <array>
#include <cstdio>

#include "analysis/disasm68k/disasm68k.hpp"
#include "analysis/disasmz80/disasmz80.hpp"
#include "core/sched/machine.hpp"
#include "debug/memory.hpp"

namespace gen::debug {

namespace {

std::uint32_t addrMask(CpuId cpu) {
    return cpu == CpuId::Z80 ? 0xFFFF : 0xFFFFFF;
}

// Valid instructions in a row ending right before `addr`, looking back at most `depth` instructions.
int chainScore(const Machine& m, CpuId cpu, std::uint32_t addr, int depth, const KnownStarts& known) {
    if (depth == 0) {
        return 0;
    }
    const int step = cpu == CpuId::Z80 ? 1 : 2;
    const int maxLen = cpu == CpuId::Z80 ? 4 : 10;
    int best = 0;
    for (int len = step; len <= maxLen; len += step) {
        if (addr < static_cast<std::uint32_t>(len)) {
            break;
        }
        const std::uint32_t p = addr - static_cast<std::uint32_t>(len);
        const ListingLine l = disassembleAt(m, cpu, p);
        if (!l.valid || l.length != len) {
            continue;
        }
        if (known.contains(cpu, p)) {
            return 100;
        }
        best = std::max(best, 1 + chainScore(m, cpu, p, depth - 1, known));
    }
    return best;
}

}  // namespace

ListingLine disassembleAt(const Machine& m, CpuId cpu, std::uint32_t addr) {
    addr &= addrMask(cpu);
    std::array<std::uint8_t, 10> bytes{};
    std::size_t n = 0;
    for (; n < bytes.size(); ++n) {
        const auto b = peek(m, cpu, addr + static_cast<std::uint32_t>(n));
        if (!b) {
            break;
        }
        bytes[n] = *b;
    }
    ListingLine line;
    line.addr = addr;
    if (n == 0) {
        line.text = "(not memory)";
        line.valid = false;
        line.length = cpu == CpuId::Z80 ? 1 : 2;
        return line;
    }
    const std::span<const std::uint8_t> span(bytes.data(), n);
    const analysis::disasm68k::Instruction in =
        cpu == CpuId::Z80 ? analysis::disasmz80::disassemble(span, static_cast<std::uint16_t>(addr))
                          : analysis::disasm68k::disassemble(span, addr);
    line.length = std::max<std::uint8_t>(in.length, 1);
    line.valid = in.valid();
    line.text = in.text();
    std::replace(line.text.begin(), line.text.end(), '\t', ' ');
    char buf[4];
    for (std::size_t i = 0; i < line.length && i < n; ++i) {
        std::snprintf(buf, sizeof buf, "%02X", bytes[i]);
        line.bytes += buf;
        if (cpu == CpuId::Z80 || (i & 1) == 1) {
            line.bytes += ' ';
        }
    }
    return line;
}

std::uint32_t previousStart(const Machine& m, CpuId cpu, std::uint32_t addr, const KnownStarts& known) {
    const int step = cpu == CpuId::Z80 ? 1 : 2;
    const int maxLen = cpu == CpuId::Z80 ? 4 : 10;
    if (addr < static_cast<std::uint32_t>(step)) {
        return 0;
    }
    std::uint32_t best = addr - static_cast<std::uint32_t>(step);
    int bestScore = -1;
    for (int len = step; len <= maxLen; len += step) {
        if (addr < static_cast<std::uint32_t>(len)) {
            break;
        }
        const std::uint32_t p = addr - static_cast<std::uint32_t>(len);
        const ListingLine l = disassembleAt(m, cpu, p);
        if (l.length != len) {
            continue;
        }
        int score = 0;
        if (known.contains(cpu, p)) {
            score = 1000;
        } else if (l.valid) {
            score = 1 + chainScore(m, cpu, p, 3, known);
        }
        // Ties go to the shorter instruction (the more common case in real code).
        if (score > bestScore) {
            bestScore = score;
            best = p;
        }
    }
    return best;
}

std::vector<ListingLine> buildListing(const Machine& m, CpuId cpu, std::uint32_t anchor, int before, int count,
                                      const KnownStarts& known) {
    std::uint32_t start = anchor & addrMask(cpu);
    for (int i = 0; i < before && start > 0; ++i) {
        start = previousStart(m, cpu, start, known);
    }
    std::vector<ListingLine> lines;
    lines.reserve(static_cast<std::size_t>(std::max(count, 0)));
    std::uint32_t at = start;
    for (int i = 0; i < count; ++i) {
        lines.push_back(disassembleAt(m, cpu, at));
        at = (at + lines.back().length) & addrMask(cpu);
    }
    return lines;
}

}  // namespace gen::debug
