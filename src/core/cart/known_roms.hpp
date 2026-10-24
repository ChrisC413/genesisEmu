#pragma once

#include <cstdint>
#include <span>
#include <string_view>

namespace gen::cart {

enum class Game { Sonic1, Sonic2, Sonic3, SonicAndKnuckles, Sonic3AndKnuckles };

struct KnownRom {
    std::string_view sha1;  // lowercase hex
    std::uint32_t crc32;
    std::uint32_t size;
    Game game;
    std::string_view title;
    std::string_view revision;  // "REV00", "REV01", ...
    std::string_view region;
};

// All target-game revisions this build recognises. Q5 (which revisions are canonical) is still open,
// so every retail revision is listed.
std::span<const KnownRom> knownRoms();

// Looks up a ROM by lowercase hex SHA-1; nullptr if unknown.
const KnownRom* identifyRom(std::string_view sha1Hex);

std::string_view toString(Game game);

}  // namespace gen::cart
