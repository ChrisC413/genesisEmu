#include "core/cart/rom.hpp"

#include <algorithm>
#include <fstream>
#include <set>
#include <string_view>

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

TEST_CASE("Plain images named .smd are detected as BIN by content", "[cart]") {
    // Whole-16 KB sizes can never be SMD (no 512-byte header).
    const auto plain = makeSyntheticRom();
    const auto loaded = loadRom(plain, "misnamed.smd");
    REQUIRE(loaded);
    CHECK(loaded.rom->sourceFormat == RomFormat::Bin);
    CHECK(loaded.rom->data == plain);

    // 512 + n * 16 KB bytes with "SEGA" at $100 in the plain layout: still a plain image.
    SyntheticRomOptions odd;
    odd.size = kSmdHeaderSize + 2 * kSmdBlockSize;
    const auto oddPlain = makeSyntheticRom(odd);
    CHECK_FALSE(isSmd(oddPlain, ".smd"));
    const auto oddLoaded = loadRom(oddPlain, "odd.smd");
    REQUIRE(oddLoaded);
    CHECK(oddLoaded.rom->sourceFormat == RomFormat::Bin);

    // A real SMD without the $AA $BB signature is still found by its de-interleaved header.
    auto smd = encodeSmd(makeSyntheticRom());
    smd[8] = 0;
    smd[9] = 0;
    CHECK(isSmd(smd, ".bin"));
}

TEST_CASE("Lock-on combinations are identified by their combined hash", "[cart]") {
    const std::vector<std::uint8_t> a(0x10, 0x11);
    const std::vector<std::uint8_t> b(0x10, 0x22);
    const auto combined = lockOnImage(a, b);
    REQUIRE(combined.size() == 0x20);
    CHECK(combined[0x0F] == 0x11);
    CHECK(combined[0x10] == 0x22);
    CHECK(identifyLockOn(a, b) == nullptr);
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
    CHECK(count(Game::SonicAndKnuckles) == 1);
    CHECK(count(Game::Sonic3AndKnuckles) >= 1);
    CHECK(identifyRom("0000000000000000000000000000000000000000") == nullptr);
}

TEST_CASE("Master System and other non-Genesis images are rejected", "[cart]") {
    const auto putTmrSega = [](std::vector<std::uint8_t>& image, std::size_t offset) {
        constexpr std::string_view kMagic = "TMR SEGA";
        std::copy(kMagic.begin(), kMagic.end(), image.begin() + static_cast<std::ptrdiff_t>(offset));
    };

    SyntheticRomOptions sms;
    sms.size = 0x40000;
    sms.consoleName = "";  // SMS images have Z80 code at $100, not a console name
    auto smsImage = makeSyntheticRom(sms);
    putTmrSega(smsImage, 0x7FF0);
    const auto rejected = loadRom(smsImage, "game.bin");
    CHECK_FALSE(rejected);
    CHECK(rejected.error.find("Not a Mega Drive/Genesis ROM") != std::string::npos);
    CHECK(rejected.error.find("TMR SEGA") != std::string::npos);

    // No SMS header, but an SMS extension and no "SEGA" console name.
    const auto byExtension = loadRom(makeSyntheticRom(sms), "game.sms");
    CHECK_FALSE(byExtension);
    CHECK(byExtension.error.find("'.sms'") != std::string::npos);

    // A real Genesis header wins: "TMR SEGA" bytes in the body do not reject it.
    auto genesis = makeSyntheticRom();
    putTmrSega(genesis, 0x7FF0);
    CHECK(loadRom(genesis, "game.bin"));

    // A header-less image without SMS markers still loads, with a warning.
    const auto unknown = loadRom(makeSyntheticRom(sms), "game.bin");
    REQUIRE(unknown);
    CHECK_FALSE(unknown.rom->warnings.empty());
}
