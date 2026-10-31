// Cartridge device chain, SRAM and $A130F1 (M04-08).

#include <cstdint>
#include <memory>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "common/sram_file.hpp"
#include "core/cart/rom.hpp"
#include "core/sched/machine.hpp"
#include "support/synthetic_rom.hpp"
#include "test_support.hpp"

using namespace gen;

namespace {

Machine sramMachine(std::size_t romSize) {
    test::SyntheticRomOptions o;
    o.size = romSize;
    o.declareSram = true;  // odd bytes at $200001-$203FFF, like S3K
    return Machine(test::makeSyntheticRom(o), cart::kRegionAmericas);
}

class LedDevice final : public cart::ICartDevice {
public:
    std::string_view name() const override { return "test-led"; }
    bool timeWrite(std::uint8_t reg, std::uint8_t value) override {
        if (reg != 0xE1) {
            return false;
        }
        writes.push_back(value);
        return true;
    }
    std::vector<std::uint8_t> writes;
};

}  // namespace

TEST_CASE("SRAM over a 4 MB ROM is switched by $A130F1", "[cart][sram]") {
    Machine m = sramMachine(0x400000);
    REQUIRE(m.sram() != nullptr);
    auto& b = m.bus();
    const std::uint8_t romByte = m.rom()[0x200001];
    CHECK_FALSE(m.sram()->mapped());
    CHECK(b.read8(0x200001) == romByte);  // ROM visible until enabled
    b.write8(0xA130F1, 0x01);
    CHECK(b.read8(0x200001) == 0xFF);  // blank SRAM
    b.write8(0x200001, 0x42);
    b.write8(0x203FFF, 0x99);
    CHECK(b.read8(0x200001) == 0x42);
    CHECK(b.read8(0x203FFF) == 0x99);
    CHECK(m.sram()->data().size() == 0x2000);
    CHECK(m.sram()->dirty());
    CHECK(m.bus().issueCount(bus::BusIssue::RomWrite) == 0);
    b.write8(0x200000, 0x55);  // even lane: not wired
    CHECK(m.bus().issueCount(bus::BusIssue::RomWrite) == 1);
    b.write8(0xA130F1, 0x03);  // write protect
    b.write8(0x200001, 0x11);
    CHECK(b.read8(0x200001) == 0x42);
    b.write8(0xA130F1, 0x00);  // disable: ROM again, and a flush is requested
    CHECK(b.read8(0x200001) == romByte);
    CHECK(m.sram()->flushRequested());
    // Contents survive a console reset; the control register does not.
    m.hardReset();
    CHECK_FALSE(m.sram()->mapped());
    b.write8(0xA130F1, 0x01);
    CHECK(b.read8(0x200001) == 0x42);
}

TEST_CASE("SRAM beyond a small ROM is mapped from power on", "[cart][sram]") {
    Machine m = sramMachine(0x20000);
    REQUIRE(m.sram() != nullptr);
    CHECK(m.sram()->mapped());
    m.bus().write8(0x200001, 0x7E);
    CHECK((m.bus().read16(0x200000) & 0xFF) == 0x7E);  // odd lane only
}

TEST_CASE("SRAM persists to a .srm file", "[cart][sram]") {
    test::TempDir dir;
    const auto path = common::sramPath(common::AppPaths(dir.path()), "abc123");
    {
        Machine m = sramMachine(0x20000);
        m.bus().write8(0x200001, 0x5A);
        REQUIRE(common::saveSram(*m.sram(), path));
        CHECK_FALSE(m.sram()->dirty());
    }
    Machine m2 = sramMachine(0x20000);
    REQUIRE(common::loadSram(*m2.sram(), path));
    CHECK(m2.bus().read8(0x200001) == 0x5A);
}

TEST_CASE("A device claims its /TIME register; unclaimed $A130xx writes are logged", "[cart][devices]") {
    Machine m(test::makeSyntheticRom(), cart::kRegionAmericas);
    CHECK(m.sram() == nullptr);
    auto led = std::make_unique<LedDevice>();
    LedDevice* ledPtr = led.get();
    m.addCartDevice(std::move(led));
    m.bus().write8(0xA130E1, 0x7F);
    m.bus().write16(0xA130E0, 0x0003);  // a word write reaches the odd register through the low byte
    CHECK(ledPtr->writes == std::vector<std::uint8_t>{0x7F, 0x03});
    CHECK(m.bus().issueCount(bus::BusIssue::Unmapped) == 0);
    m.bus().write8(0xA130E3, 0x01);
    CHECK(m.bus().issueCount(bus::BusIssue::Unmapped) == 1);
    CHECK(m.bus().issues().back().addr == 0xA130E3);
}
