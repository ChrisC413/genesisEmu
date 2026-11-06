#include "debug/memory.hpp"

#include <algorithm>
#include <cctype>

#include "core/sched/machine.hpp"

namespace gen::debug {

std::optional<std::uint8_t> peek(const Machine& m, CpuId cpu, std::uint32_t addr) {
    if (cpu == CpuId::Z80) {
        addr &= 0xFFFF;
        if (addr < 0x4000) {
            return m.z80().state().ram[addr & 0x1FFF];
        }
        return std::nullopt;
    }
    addr &= 0xFFFFFF;
    if (const std::uint8_t* page = m.bus().fastRead[addr >> 16]) {
        return page[addr & 0xFFFF];
    }
    if (addr >= 0xE00000) {
        return m.bus().state().ram[addr & 0xFFFF];
    }
    if (const cart::SramDevice* sram = m.sram();
        sram != nullptr && sram->mapped() && addr >= sram->memFirst() && addr <= sram->memLast()) {
        std::uint8_t v = 0;
        if (const_cast<cart::SramDevice*>(sram)->memRead(addr, v)) {  // a plain array read
            return v;
        }
    }
    if (addr < m.rom().size()) {
        return m.rom()[addr];
    }
    return std::nullopt;
}

std::uint32_t peekValue(const Machine& m, CpuId cpu, std::uint32_t addr, int size) {
    std::uint32_t v = 0;
    for (int i = 0; i < size; ++i) {
        const std::uint32_t b = peek(m, cpu, addr + static_cast<std::uint32_t>(i)).value_or(0);
        if (cpu == CpuId::Z80) {
            v |= b << (8 * i);
        } else {
            v = (v << 8) | b;
        }
    }
    return v;
}

RegionInfo regionInfo(Region region) {
    switch (region) {
        case Region::Rom:
            return {"ROM", 0};
        case Region::Ram68k:
            return {"68k RAM", 0xFF0000};
        case Region::RamZ80:
            return {"Z80 RAM", 0};
        case Region::Vram:
            return {"VRAM", 0};
        case Region::Cram:
            return {"CRAM", 0};
        case Region::Vsram:
            return {"VSRAM", 0};
        case Region::Sram:
            return {"SRAM", 0};
        case Region::Count:
            break;
    }
    return {"?", 0};
}

std::size_t regionSize(const Machine& m, Region region) {
    switch (region) {
        case Region::Rom:
            return m.rom().size();
        case Region::Ram68k:
            return m.bus().state().ram.size();
        case Region::RamZ80:
            return m.z80().state().ram.size();
        case Region::Vram:
            return vdp::kVramSize;
        case Region::Cram:
            return vdp::kCramWords * 2;
        case Region::Vsram:
            return vdp::kVsramWords * 2;
        case Region::Sram:
            return m.sram() != nullptr ? m.sram()->data().size() : 0;
        case Region::Count:
            break;
    }
    return 0;
}

std::optional<std::uint8_t> readRegion(const Machine& m, Region region, std::size_t offset) {
    if (offset >= regionSize(m, region)) {
        return std::nullopt;
    }
    const auto wordByte = [&](std::uint16_t w) {
        return static_cast<std::uint8_t>((offset & 1) == 0 ? w >> 8 : w);
    };
    switch (region) {
        case Region::Rom:
            return m.rom()[offset];
        case Region::Ram68k:
            return m.bus().state().ram[offset];
        case Region::RamZ80:
            return m.z80().state().ram[offset];
        case Region::Vram:
            return m.vdp().state().vram[offset];
        case Region::Cram:
            return wordByte(m.vdp().state().cram[offset / 2]);
        case Region::Vsram:
            return wordByte(m.vdp().state().vsram[offset / 2]);
        case Region::Sram:
            return m.sram()->data()[offset];
        case Region::Count:
            break;
    }
    return std::nullopt;
}

bool writeRegion(Machine& m, Region region, std::size_t offset, std::uint8_t value,
                 std::vector<RomPatch>& patches) {
    if (offset >= regionSize(m, region)) {
        return false;
    }
    const auto setWordByte = [&](std::uint16_t& w, std::uint16_t validBits) {
        const std::uint16_t v = (offset & 1) == 0 ? static_cast<std::uint16_t>((w & 0x00FF) | (value << 8))
                                                  : static_cast<std::uint16_t>((w & 0xFF00) | value);
        w = static_cast<std::uint16_t>(v & validBits);
    };
    switch (region) {
        case Region::Rom: {
            const auto addr = static_cast<std::uint32_t>(offset);
            const auto it = std::find_if(patches.begin(), patches.end(),
                                         [&](const RomPatch& p) { return p.addr == addr; });
            const std::uint8_t original = it != patches.end() ? it->original : m.rom()[offset];
            if (value == original) {
                if (it != patches.end()) {
                    patches.erase(it);
                }
            } else if (it != patches.end()) {
                it->value = value;
            } else {
                patches.push_back({addr, original, value});
            }
            m.patchRom(addr, value);
            return true;
        }
        case Region::Ram68k:
            m.bus().state().ram[offset] = value;
            return true;
        case Region::RamZ80:
            m.z80().state().ram[offset] = value;
            return true;
        case Region::Vram:
            m.vdp().state().vram[offset] = value;
            return true;
        case Region::Cram:
            setWordByte(m.vdp().state().cram[offset / 2], 0x0EEE);
            m.vdp().invalidateCaches();
            return true;
        case Region::Vsram:
            setWordByte(m.vdp().state().vsram[offset / 2], 0x07FF);
            return true;
        case Region::Sram: {
            std::vector<std::uint8_t> data = m.sram()->data();
            data[offset] = value;
            m.sram()->load(data);
            return true;
        }
        case Region::Count:
            break;
    }
    return false;
}

std::optional<BytePattern> parsePattern(std::string_view text, std::string& error) {
    BytePattern p;
    std::string digits;
    for (const char c : text) {
        if (std::isspace(static_cast<unsigned char>(c)) != 0 || c == ',') {
            continue;
        }
        if (c == '$') {
            continue;
        }
        if (c != '?' && std::isxdigit(static_cast<unsigned char>(c)) == 0) {
            error = std::string("'") + c + "' is not a hex digit or ?";
            return std::nullopt;
        }
        digits += c;
    }
    if (digits.empty()) {
        error = "empty pattern";
        return std::nullopt;
    }
    if (digits.size() % 2 != 0) {
        error = "odd number of hex digits";
        return std::nullopt;
    }
    for (std::size_t i = 0; i < digits.size(); i += 2) {
        const std::string pair = digits.substr(i, 2);
        if (pair == "??") {
            p.bytes.push_back(0);
            p.wildcard.push_back(true);
            continue;
        }
        if (pair.find('?') != std::string::npos) {
            error = "'" + pair + "': use ?? for a whole wildcard byte";
            return std::nullopt;
        }
        p.bytes.push_back(static_cast<std::uint8_t>(std::stoul(pair, nullptr, 16)));
        p.wildcard.push_back(false);
    }
    return p;
}

std::vector<std::size_t> findAll(std::span<const std::uint8_t> data, const BytePattern& pattern,
                                 std::size_t limit) {
    std::vector<std::size_t> hits;
    const std::size_t n = pattern.bytes.size();
    if (n == 0 || data.size() < n) {
        return hits;
    }
    // Anchor on the first literal byte with memchr-like scanning.
    std::size_t anchor = 0;
    while (anchor < n && pattern.wildcard[anchor]) {
        ++anchor;
    }
    for (std::size_t i = 0; i + n <= data.size() && hits.size() < limit; ++i) {
        if (anchor < n && data[i + anchor] != pattern.bytes[anchor]) {
            continue;
        }
        bool match = true;
        for (std::size_t k = 0; k < n; ++k) {
            if (!pattern.wildcard[k] && data[i + k] != pattern.bytes[k]) {
                match = false;
                break;
            }
        }
        if (match) {
            hits.push_back(i);
        }
    }
    return hits;
}

std::vector<std::uint8_t> snapshot(const Machine& m, Region region) {
    const std::size_t n = regionSize(m, region);
    std::vector<std::uint8_t> out(n);
    switch (region) {
        case Region::Rom:
            std::copy_n(m.rom().begin(), n, out.begin());
            break;
        case Region::Ram68k:
            std::copy_n(m.bus().state().ram.begin(), n, out.begin());
            break;
        case Region::RamZ80:
            std::copy_n(m.z80().state().ram.begin(), n, out.begin());
            break;
        case Region::Vram:
            std::copy_n(m.vdp().state().vram.begin(), n, out.begin());
            break;
        default:
            for (std::size_t i = 0; i < n; ++i) {
                out[i] = readRegion(m, region, i).value_or(0);
            }
            break;
    }
    return out;
}

std::vector<std::size_t> findAll(const Machine& m, Region region, const BytePattern& pattern,
                                 std::size_t limit) {
    if (region == Region::Rom) {
        return findAll(std::span<const std::uint8_t>(m.rom()), pattern, limit);
    }
    const std::vector<std::uint8_t> data = snapshot(m, region);
    return findAll(std::span<const std::uint8_t>(data), pattern, limit);
}

}  // namespace gen::debug
