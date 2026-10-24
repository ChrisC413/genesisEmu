// [rom] tests run against the user's own ROM dumps in GENESIS_ROM_DIR and are skipped without it.

#include <filesystem>
#include <set>
#include <string>

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include "common/paths.hpp"
#include "core/cart/rom.hpp"

namespace fs = std::filesystem;
using namespace gen::cart;

TEST_CASE("User ROMs in GENESIS_ROM_DIR are identified by name and revision", "[rom]") {
    const auto romDir = gen::common::getEnvPath("GENESIS_ROM_DIR");
    if (!romDir || !fs::is_directory(*romDir)) {
        SKIP("GENESIS_ROM_DIR is not set or not a directory");
    }

    std::set<std::string> identified;
    for (const auto& entry : fs::recursive_directory_iterator(*romDir)) {
        if (!entry.is_regular_file() || entry.file_size() > kMaxRomSize + kSmdHeaderSize) {
            continue;
        }
        const auto result = loadRomFile(entry.path());
        if (!result || result.rom->known == nullptr) {
            continue;
        }
        const Rom& rom = *result.rom;
        INFO(entry.path().string());
        CHECK(rom.data.size() == rom.known->size);
        CHECK(rom.crc32 == rom.known->crc32);
        identified.insert(std::string(toString(rom.known->game)) + " " + std::string(rom.known->revision) + " (" +
                          std::string(rom.known->region) + ")");
    }

    if (identified.empty()) {
        SKIP("No known target-game ROMs found in GENESIS_ROM_DIR");
    }
    for (const auto& name : identified) {
        UNSCOPED_INFO("identified: " << name);
    }
    SUCCEED();
}
