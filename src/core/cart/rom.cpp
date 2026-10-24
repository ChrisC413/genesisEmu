#include "core/cart/rom.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <system_error>

namespace gen::cart {

namespace {

std::uint32_t readBe32(std::span<const std::uint8_t> d, std::size_t at) {
    return (std::uint32_t{d[at]} << 24) | (std::uint32_t{d[at + 1]} << 16) | (std::uint32_t{d[at + 2]} << 8) |
           std::uint32_t{d[at + 3]};
}

std::string readText(std::span<const std::uint8_t> d, std::size_t at, std::size_t len) {
    std::string s;
    s.reserve(len);
    for (std::size_t i = 0; i < len; ++i) {
        const std::uint8_t c = d[at + i];
        s.push_back(c >= 0x20 && c < 0x7F ? static_cast<char>(c) : (c == 0 ? ' ' : '?'));
    }
    const auto last = s.find_last_not_of(' ');
    s.erase(last == std::string::npos ? 0 : last + 1);
    const auto first = s.find_first_not_of(' ');
    s.erase(0, first == std::string::npos ? s.size() : first);
    return s;
}

std::uint8_t parseRegions(std::span<const std::uint8_t> d) {
    // Old style: any of the letters J/U/E. New style: a single hex digit bitmask.
    std::uint8_t mask = kRegionNone;
    bool oldStyle = false;
    for (std::size_t i = 0; i < 3; ++i) {
        switch (d[0x1F0 + i]) {
            case 'J':
                mask |= kRegionJapan;
                oldStyle = true;
                break;
            case 'U':
                mask |= kRegionAmericas;
                oldStyle = true;
                break;
            case 'E':
                mask |= kRegionEurope;
                oldStyle = true;
                break;
            default:
                break;
        }
    }
    if (oldStyle) {
        return mask;
    }
    const int c = d[0x1F0];
    if (std::isxdigit(c)) {
        const int v = std::isdigit(c) ? c - '0' : std::toupper(c) - 'A' + 10;
        return static_cast<std::uint8_t>(v & (kRegionJapan | kRegionAmericas | kRegionEurope));
    }
    return kRegionNone;
}

std::string utf8(const std::filesystem::path& path) {
    const std::u8string u8 = path.u8string();
    return {u8.begin(), u8.end()};
}

std::string lowercaseExtension(std::string_view fileName) {
    const auto dot = fileName.rfind('.');
    if (dot == std::string_view::npos) {
        return {};
    }
    std::string ext(fileName.substr(dot));
    std::ranges::transform(ext, ext.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return ext;
}

}  // namespace

bool isSmd(std::span<const std::uint8_t> file, std::string_view extension) {
    if (file.size() < kSmdHeaderSize + kSmdBlockSize || (file.size() - kSmdHeaderSize) % kSmdBlockSize != 0) {
        return false;
    }
    const bool signature = file[8] == 0xAA && file[9] == 0xBB;
    return signature || extension == ".smd";
}

std::vector<std::uint8_t> deinterleaveSmd(std::span<const std::uint8_t> file) {
    const auto body = file.subspan(kSmdHeaderSize);
    std::vector<std::uint8_t> out(body.size());
    constexpr std::size_t half = kSmdBlockSize / 2;
    for (std::size_t block = 0; block < body.size(); block += kSmdBlockSize) {
        for (std::size_t i = 0; i < half; ++i) {
            out[block + i * 2] = body[block + half + i];
            out[block + i * 2 + 1] = body[block + i];
        }
    }
    return out;
}

RomHeader parseHeader(std::span<const std::uint8_t> d) {
    RomHeader h;
    h.consoleName = readText(d, 0x100, 16);
    h.copyright = readText(d, 0x110, 16);
    h.domesticTitle = readText(d, 0x120, 48);
    h.overseasTitle = readText(d, 0x150, 48);
    h.serial = readText(d, 0x180, 14);
    h.checksum = static_cast<std::uint16_t>((d[0x18E] << 8) | d[0x18F]);
    h.ioSupport = readText(d, 0x190, 16);
    h.romRange = {readBe32(d, 0x1A0), readBe32(d, 0x1A4)};
    h.ramRange = {readBe32(d, 0x1A8), readBe32(d, 0x1AC)};

    SramDeclaration& sram = h.sram;
    sram.declared = d[0x1B0] == 'R' && d[0x1B1] == 'A';
    if (sram.declared) {
        // $1B2 = %1x1yz000: x = backup, yz = 00 both lanes, 10 even, 11 odd.
        sram.typeByte = d[0x1B2];
        sram.backup = (sram.typeByte & 0x40) != 0;
        switch ((sram.typeByte >> 3) & 0x3) {
            case 0x2:
                sram.lanes = SramLanes::Even;
                break;
            case 0x3:
                sram.lanes = SramLanes::Odd;
                break;
            default:
                sram.lanes = SramLanes::Both;
                break;
        }
        sram.range = {readBe32(d, 0x1B4), readBe32(d, 0x1B8)};
    }

    h.modem = readText(d, 0x1BC, 12);
    h.notes = readText(d, 0x1C8, 40);
    h.regionCodes = readText(d, 0x1F0, 3);
    h.regions = parseRegions(d);
    return h;
}

std::uint16_t computeChecksum(std::span<const std::uint8_t> image) {
    std::uint16_t sum = 0;
    for (std::size_t i = 0x200; i < image.size(); i += 2) {
        const std::uint16_t hi = image[i];
        const std::uint16_t lo = i + 1 < image.size() ? image[i + 1] : 0;
        sum = static_cast<std::uint16_t>(sum + ((hi << 8) | lo));
    }
    return sum;
}

LoadResult loadRom(std::vector<std::uint8_t> file, std::string_view fileName) {
    LoadResult result;
    Rom rom;
    const std::string ext = lowercaseExtension(fileName);

    if (isSmd(file, ext)) {
        rom.sourceFormat = RomFormat::Smd;
        rom.data = deinterleaveSmd(file);
    } else {
        rom.sourceFormat = RomFormat::Bin;
        rom.data = std::move(file);
    }

    if (rom.data.size() < kMinRomSize) {
        result.error = "ROM is too small (" + std::to_string(rom.data.size()) + " bytes; minimum is " +
                       std::to_string(kMinRomSize) + " bytes)";
        return result;
    }
    if (rom.data.size() > kMaxRomSize) {
        result.error = "ROM is too large (" + std::to_string(rom.data.size()) + " bytes; maximum is " +
                       std::to_string(kMaxRomSize) + " bytes / 4 MB)";
        return result;
    }

    rom.header = parseHeader(rom.data);
    rom.computedChecksum = computeChecksum(rom.data);
    rom.sha1 = sha1(rom.data);
    rom.sha1Hex = toHex(rom.sha1);
    rom.crc32 = crc32(rom.data);
    rom.known = identifyRom(rom.sha1Hex);

    if (!ext.empty() && ext != ".bin" && ext != ".md" && ext != ".gen" && ext != ".smd") {
        rom.warnings.push_back("Unrecognised file extension '" + ext + "'; treated as a plain image");
    }
    if (!rom.header.consoleName.starts_with("SEGA")) {
        rom.warnings.push_back("Header console name does not start with \"SEGA\"");
    }
    if (!rom.checksumMatches()) {
        char buf[96];
        std::snprintf(buf, sizeof buf, "Header checksum mismatch: header $%04X, computed $%04X",
                      rom.header.checksum, rom.computedChecksum);
        rom.warnings.emplace_back(buf);
    }

    result.rom = std::move(rom);
    return result;
}

LoadResult loadRomFile(const std::filesystem::path& path) {
    LoadResult result;
    std::error_code ec;
    const auto size = std::filesystem::file_size(path, ec);
    if (ec) {
        result.error = "Cannot open '" + utf8(path) + "': " + ec.message();
        return result;
    }
    if (size > kMaxRomSize + kSmdHeaderSize) {
        result.error = "ROM is too large (" + std::to_string(size) + " bytes; maximum is " +
                       std::to_string(kMaxRomSize) + " bytes / 4 MB)";
        return result;
    }

    std::ifstream in(path, std::ios::binary);
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
    if (!in || !in.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()))) {
        result.error = "Cannot read '" + utf8(path) + "'";
        return result;
    }
    return loadRom(std::move(bytes), utf8(path.filename()));
}

std::string_view toString(RomFormat format) {
    return format == RomFormat::Smd ? "SMD (interleaved)" : "BIN";
}

std::string_view toString(SramLanes lanes) {
    switch (lanes) {
        case SramLanes::Both:
            return "both";
        case SramLanes::Even:
            return "even";
        case SramLanes::Odd:
            return "odd";
    }
    return "?";
}

std::string regionsToString(std::uint8_t regions) {
    std::string s;
    const auto add = [&s](const char* name) {
        if (!s.empty()) {
            s += ", ";
        }
        s += name;
    };
    if (regions & kRegionJapan) {
        add("Japan");
    }
    if (regions & kRegionAmericas) {
        add("Americas");
    }
    if (regions & kRegionEurope) {
        add("Europe");
    }
    return s.empty() ? "none" : s;
}

}  // namespace gen::cart
