#include "core/cart/rom.hpp"

#include <fstream>
#include <set>

#include <catch2/catch_test_macros.hpp>

#include "support/synthetic_rom.hpp"
#include "test_support.hpp"

using namespace gen::cart;
using gen::test::encodeSmd;
using gen::test::makeSyntheticRom;
using gen::test::SyntheticRomOptions;

TEST_CASE("Header fields are parsed", "[cart]") {
    SyntheticRomOptions o;
    o.domesticTitle = "DOMESTIC NAME";
    o.overseasTitle = "OVERSEAS NAME";
    o.serial = "GM 00001051-01";
    const auto image = makeSyntheticRom(o);

    const RomHeader h = parseHeader(image);
    CHECK(h.consoleName == "SEGA MEGA DRIVE");
    CHECK(h.copyright == "(C)TEST 2026.OCT");
    CHECK(h.domesticTitle == "DOMESTIC NAME");
    CHECK(h.overseasTitle == "OVERSEAS NAME");
    CHECK(h.serial == "GM 00001051-01");
    CHECK(h.ioSupport == "J");
    CHECK(h.romRange.start == 0);
    CHECK(h.romRange.end == o.size - 1);
    CHECK(h.ramRange.start == 0xFF0000);
    CHECK(h.ramRange.end == 0xFFFFFF);
    CHECK_FALSE(h.sram.declared);
    CHECK(h.regionCodes == "JUE");
    CHECK(h.regions == (kRegionJapan | kRegionAmericas | kRegionEurope));
}

TEST_CASE("SRAM declaration is parsed", "[cart]") {
    SyntheticRomOptions o;
    o.declareSram = true;  // S3K-style: backup RAM on odd bytes $200001-$203FFF
    const RomHeader h = parseHeader(makeSyntheticRom(o));
    REQUIRE(h.sram.declared);
    CHECK(h.sram.backup);
    CHECK(h.sram.lanes == SramLanes::Odd);
    CHECK(h.sram.range.start == 0x200001);
    CHECK(h.sram.range.end == 0x203FFF);

    o.sramType = 0xE0;
    CHECK(parseHeader(makeSyntheticRom(o)).sram.lanes == SramLanes::Both);
    o.sramType = 0xB0;
    const RomHeader even = parseHeader(makeSyntheticRom(o));
    CHECK(even.sram.lanes == SramLanes::Even);
    CHECK_FALSE(even.sram.backup);
}

TEST_CASE("Region codes: old letters and new hex style", "[cart]") {
    SyntheticRomOptions o;
    o.regionCodes = "U";
    CHECK(parseHeader(makeSyntheticRom(o)).regions == kRegionAmericas);
    o.regionCodes = "JE";
    CHECK(parseHeader(makeSyntheticRom(o)).regions == (kRegionJapan | kRegionEurope));
    o.regionCodes = "4";
    CHECK(parseHeader(makeSyntheticRom(o)).regions == kRegionAmericas);
    o.regionCodes = "F";
    CHECK(parseHeader(makeSyntheticRom(o)).regions == (kRegionJapan | kRegionAmericas | kRegionEurope));
    o.regionCodes = "";
    CHECK(parseHeader(makeSyntheticRom(o)).regions == kRegionNone);
}

TEST_CASE("Header checksum is computed and verified", "[cart]") {
    auto good = loadRom(makeSyntheticRom(), "good.bin");
    REQUIRE(good);
    CHECK(good.rom->checksumMatches());
    CHECK(good.rom->warnings.empty());

    SyntheticRomOptions o;
    o.correctChecksum = false;
    auto bad = loadRom(makeSyntheticRom(o), "bad.bin");
    REQUIRE(bad);  // a mismatch is reported but does not block loading
    CHECK_FALSE(bad.rom->checksumMatches());
    REQUIRE(bad.rom->warnings.size() == 1);
    CHECK(bad.rom->warnings[0].find("checksum mismatch") != std::string::npos);
}

TEST_CASE("Checksum of an odd-sized image pads the last byte", "[cart]") {
    std::vector<std::uint8_t> image(0x203, 0);
    image[0x200] = 0x12;
    image[0x201] = 0x34;
    image[0x202] = 0x56;
    CHECK(computeChecksum(image) == 0x1234 + 0x5600);
}

TEST_CASE("Interleaved SMD and plain BIN yield the same SHA-1", "[cart]") {
    SyntheticRomOptions o;
    o.size = 0x40000;
    const auto image = makeSyntheticRom(o);
    const auto smd = encodeSmd(image);
    REQUIRE(isSmd(smd, ".smd"));
    REQUIRE_FALSE(isSmd(image, ".bin"));

    const auto fromBin = loadRom(image, "game.bin");
    const auto fromSmd = loadRom(smd, "game.smd");
    REQUIRE(fromBin);
    REQUIRE(fromSmd);
    CHECK(fromBin.rom->sourceFormat == RomFormat::Bin);
    CHECK(fromSmd.rom->sourceFormat == RomFormat::Smd);
    CHECK(fromSmd.rom->data == image);
    CHECK(fromSmd.rom->sha1Hex == fromBin.rom->sha1Hex);
    CHECK(fromSmd.rom->crc32 == fromBin.rom->crc32);
    CHECK(fromSmd.rom->header.domesticTitle == "SYNTHETIC TEST ROM");
    CHECK(fromSmd.rom->checksumMatches());
}

TEST_CASE("SMD is detected by signature regardless of extension", "[cart]") {
    const auto smd = encodeSmd(makeSyntheticRom());
    CHECK(isSmd(smd, ".bin"));
    const auto loaded = loadRom(smd, "renamed.bin");
    REQUIRE(loaded);
    CHECK(loaded.rom->sourceFormat == RomFormat::Smd);
}

TEST_CASE("ROMs smaller than 512 bytes or larger than 4 MB are rejected", "[cart]") {
    SyntheticRomOptions tiny;
    tiny.size = 511;
    const auto small = loadRom(makeSyntheticRom(tiny), "tiny.bin");
    CHECK_FALSE(small);
    CHECK(small.error.find("too small") != std::string::npos);

    SyntheticRomOptions minimal;
    minimal.size = 512;
    CHECK(loadRom(makeSyntheticRom(minimal), "min.bin"));

    SyntheticRomOptions max;
    max.size = kMaxRomSize;
    CHECK(loadRom(makeSyntheticRom(max), "max.bin"));

    SyntheticRomOptions huge;
    huge.size = kMaxRomSize + 2;
    const auto large = loadRom(makeSyntheticRom(huge), "huge.bin");
    CHECK_FALSE(large);
    CHECK(large.error.find("too large") != std::string::npos);

    SyntheticRomOptions hugeSmd;
    hugeSmd.size = kMaxRomSize + kSmdBlockSize;
    const auto largeSmd = loadRom(encodeSmd(makeSyntheticRom(hugeSmd)), "huge.smd");
    CHECK_FALSE(largeSmd);
    CHECK(largeSmd.error.find("too large") != std::string::npos);
}

TEST_CASE("Loading from disk", "[cart]") {
    gen::test::TempDir dir;
    const auto image = makeSyntheticRom();
    const auto path = dir.path() / "synthetic.gen";
    {
        std::ofstream out(path, std::ios::binary);
        out.write(reinterpret_cast<const char*>(image.data()), static_cast<std::streamsize>(image.size()));
    }
    const auto loaded = loadRomFile(path);
    REQUIRE(loaded);
    CHECK(loaded.rom->data == image);
    CHECK(loaded.rom->known == nullptr);

    const auto missing = loadRomFile(dir.path() / "missing.bin");
    CHECK_FALSE(missing);
    CHECK(missing.error.find("Cannot open") != std::string::npos);
}

TEST_CASE("Known-ROM table is well formed and searchable", "[cart]") {
    std::set<std::string_view> seen;
    for (const KnownRom& k : knownRoms()) {
        CHECK(k.sha1.size() == 40);
        CHECK(k.sha1.find_first_not_of("0123456789abcdef") == std::string_view::npos);
        CHECK(seen.insert(k.sha1).second);
        CHECK(k.size <= kMaxRomSize);
        CHECK(identifyRom(k.sha1) == &k);
    }
    const auto count = [](Game g) {
        std::size_t n = 0;
        for (const KnownRom& k : knownRoms()) {
            n += k.game == g ? 1 : 0;
        }
        return n;
    };
    CHECK(count(Game::Sonic1) == 2);
    CHECK(count(Game::Sonic2) == 3);
    CHECK(count(Game::Sonic3AndKnuckles) >= 1);
    CHECK(identifyRom("0000000000000000000000000000000000000000") == nullptr);
}
