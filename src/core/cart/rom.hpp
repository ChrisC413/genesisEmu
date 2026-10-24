#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "core/cart/known_roms.hpp"
#include "core/util/hash.hpp"

namespace gen::cart {

// The de-interleaved ROM image must be within these bounds (inclusive).
inline constexpr std::size_t kMinRomSize = 0x200;
inline constexpr std::size_t kMaxRomSize = 0x400000;

inline constexpr std::size_t kSmdHeaderSize = 0x200;
inline constexpr std::size_t kSmdBlockSize = 0x4000;

enum class RomFormat { Bin, Smd };

// Bitmask of the regions a cartridge declares support for (header $1F0).
enum RegionMask : std::uint8_t {
    kRegionNone = 0,
    kRegionJapan = 1 << 0,
    kRegionAmericas = 1 << 2,
    kRegionEurope = 1 << 3,
};

struct AddressRange {
    std::uint32_t start = 0;
    std::uint32_t end = 0;
};

// Which byte lanes of the 16-bit data bus the SRAM chip is wired to.
enum class SramLanes { Both, Even, Odd };

struct SramDeclaration {
    bool declared = false;  // "RA" marker present at $1B0
    bool backup = false;    // battery-backed (bit 6 of $1B2)
    std::uint8_t typeByte = 0;
    SramLanes lanes = SramLanes::Both;
    AddressRange range;
};

// Parsed cartridge header ($100-$1FF). Text fields are trimmed; non-printable bytes become '?'.
struct RomHeader {
    std::string consoleName;     // $100
    std::string copyright;       // $110
    std::string domesticTitle;   // $120
    std::string overseasTitle;   // $150
    std::string serial;          // $180 (product type + code + revision, e.g. "GM 00001051-01")
    std::uint16_t checksum = 0;  // $18E
    std::string ioSupport;       // $190
    AddressRange romRange;       // $1A0
    AddressRange ramRange;       // $1A8
    SramDeclaration sram;        // $1B0
    std::string modem;           // $1BC
    std::string notes;           // $1C8
    std::string regionCodes;     // $1F0
    std::uint8_t regions = kRegionNone;
};

struct Rom {
    std::vector<std::uint8_t> data;  // de-interleaved image
    RomFormat sourceFormat = RomFormat::Bin;
    RomHeader header;
    std::uint16_t computedChecksum = 0;
    Sha1Digest sha1{};
    std::string sha1Hex;
    std::uint32_t crc32 = 0;
    const KnownRom* known = nullptr;  // nullptr when the SHA-1 is not in the known-ROM table
    std::vector<std::string> warnings;

    bool checksumMatches() const { return computedChecksum == header.checksum; }
};

struct LoadResult {
    std::optional<Rom> rom;
    std::string error;  // set when rom is empty

    explicit operator bool() const { return rom.has_value(); }
};

// True if `file` looks like an SMD dump: a 512-byte header followed by whole 16 KB blocks, and either the
// $AA $BB signature at offsets 8-9 or a "SEGA" console name only in the de-interleaved layout. The .smd
// extension only breaks ties (many ".smd" files are plain images).
bool isSmd(std::span<const std::uint8_t> file, std::string_view extension);

// Converts an SMD file (header + interleaved blocks) to a plain image. Each 16 KB block stores the odd
// bytes in its first half and the even bytes in its second half.
std::vector<std::uint8_t> deinterleaveSmd(std::span<const std::uint8_t> file);

// Requires image.size() >= kMinRomSize.
RomHeader parseHeader(std::span<const std::uint8_t> image);

// Sum of big-endian words from $200 to the end of the image (a trailing odd byte is the high byte).
std::uint16_t computeChecksum(std::span<const std::uint8_t> image);

// Builds a Rom from raw file contents. `fileName` is only used for its extension.
LoadResult loadRom(std::vector<std::uint8_t> file, std::string_view fileName);
LoadResult loadRomFile(const std::filesystem::path& path);

// Sonic & Knuckles lock-on: the combined image is the locking cartridge (S&K, 2 MB) followed by the
// locked-on cartridge (e.g. Sonic 3). Loading lock-on pairs is backlog BL-03; these helpers only identify
// one.
std::vector<std::uint8_t> lockOnImage(std::span<const std::uint8_t> lockingCart,
                                      std::span<const std::uint8_t> lockedOnCart);
const KnownRom* identifyLockOn(std::span<const std::uint8_t> lockingCart,
                               std::span<const std::uint8_t> lockedOnCart);

std::string_view toString(RomFormat format);
std::string_view toString(SramLanes lanes);
std::string regionsToString(std::uint8_t regions);

}  // namespace gen::cart
