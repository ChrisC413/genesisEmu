// [rom] tests run against the user's own ROM dumps in GENESIS_ROM_DIR and are skipped without it.

#include <filesystem>
#include <optional>
#include <set>
#include <string>
#include <vector>

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
        if (!result) {
            UNSCOPED_INFO("rejected: " << gen::common::toUtf8(entry.path().filename()) << ": "
                                       << result.error);
            continue;
        }
        if (result.rom->known == nullptr) {
            UNSCOPED_INFO("unidentified: " << gen::common::toUtf8(entry.path().filename()) << " (SHA-1 "
                                           << result.rom->sha1Hex << ", serial '" << result.rom->header.serial
                                           << "', checksum " << (result.rom->checksumMatches() ? "ok" : "bad")
                                           << ")");
            continue;
        }
        const Rom& rom = *result.rom;
        INFO(gen::common::toUtf8(entry.path()));
        CHECK(rom.data.size() == rom.known->size);
        CHECK(rom.crc32 == rom.known->crc32);
        identified.insert(std::string(toString(rom.known->game)) + " " + std::string(rom.known->revision) +
                          " (" + std::string(rom.known->region) + ")");
    }

    if (identified.empty()) {
        SKIP("No known target-game ROMs found in GENESIS_ROM_DIR");
    }
    for (const auto& name : identified) {
        UNSCOPED_INFO("identified: " << name);
    }
    SUCCEED();
}

TEST_CASE("Sonic & Knuckles locked onto Sonic 3 hashes to the known combined images", "[rom]") {
    const auto romDir = gen::common::getEnvPath("GENESIS_ROM_DIR");
    if (!romDir || !fs::is_directory(*romDir)) {
        SKIP("GENESIS_ROM_DIR is not set or not a directory");
    }

    std::optional<Rom> sonicAndKnuckles;
    std::vector<Rom> sonic3;
    for (const auto& entry : fs::recursive_directory_iterator(*romDir)) {
        if (!entry.is_regular_file() || entry.file_size() > kMaxRomSize + kSmdHeaderSize) {
            continue;
        }
        auto result = loadRomFile(entry.path());
        if (!result || result.rom->known == nullptr) {
            continue;
        }
        if (result.rom->known->game == Game::SonicAndKnuckles && !sonicAndKnuckles) {
            sonicAndKnuckles = std::move(*result.rom);
        } else if (result.rom->known->game == Game::Sonic3) {
            sonic3.push_back(std::move(*result.rom));
        }
    }
    if (!sonicAndKnuckles || sonic3.empty()) {
        SKIP("Needs a standalone Sonic & Knuckles and a Sonic 3 dump");
    }

    for (const Rom& s3 : sonic3) {
        const KnownRom* combined = identifyLockOn(sonicAndKnuckles->data, s3.data);
        INFO("Sonic 3 " << s3.known->region);
        REQUIRE(combined != nullptr);
        CHECK(combined->game == Game::Sonic3AndKnuckles);
        CHECK(combined->region == s3.known->region);
        UNSCOPED_INFO("lock-on: S&K + Sonic 3 (" << s3.known->region << ") = " << combined->title << " ("
                                                 << combined->region << "), SHA-1 " << combined->sha1);
    }
    SUCCEED();
}
