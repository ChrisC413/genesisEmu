#pragma once
// Finds the user's ROM images in GENESIS_ROM_DIR by SHA-1 (shared by the [rom] tests).

#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace gen::test {

struct GameImage {
    std::vector<std::uint8_t> image;
    std::uint8_t regions = 0;
};

// Every known ROM image in `dir` by SHA-1, plus the S&K lock-on combinations (by combined SHA-1).
std::map<std::string, GameImage> scanRoms(const std::filesystem::path& dir);

}  // namespace gen::test
