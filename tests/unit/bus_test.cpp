// 68k memory map, I/O and Z80 bus control (stories M02-01, M02-02, M02-03; spec 04).

#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "core/cart/rom.hpp"
#include "core/sched/machine.hpp"
#include "support/synthetic_rom.hpp"

using namespace gen;

namespace {

std::vector<std::uint8_t> romWithCode(std::initializer_list<std::uint16_t> code) {
    auto rom = test::makeSyntheticRom();
    std::uint32_t at = 0x200;
    for (const std::uint16_t w : code) {
        rom[at++] = static_cast<std::uint8_t>(w >> 8);
        rom[at++] = static_cast<std::uint8_t>(w);
    }
    return rom;
}

Machine makeMachine(MachineConfig config = {}) {
    return Machine(test::makeSyntheticRom(), cart::kRegionAmericas, config);
}

std::uint64_t issues(const Machine& m, bus::BusIssue kind) {
    return m.bus().issueCount(kind);
}

}  // namespace

TEST_CASE("Cartridge ROM is readable through fast pages and ignores writes", "[bus]") {
    Machine m = makeMachine();
    auto& b = m.bus();
    CHECK(b.read16(0x100) == 0x5345);  // "SE" of "SEGA MEGA DRIVE"
    CHECK(b.read8(0x101) == 0x45);
    CHECK(b.fastRead[0x00] != nullptr);
    CHECK(b.fastRead[0x01] != nullptr);
    CHECK(b.fastWrite[0x00] == nullptr);
    b.write16(0x100, 0xFFFF);
    CHECK(b.read16(0x100) == 0x5345);
    CHECK(issues(m, bus::BusIssue::RomWrite) == 1);
}

TEST_CASE("Space past the ROM image, expansion and 32X areas are open bus and logged", "[bus]") {
    Machine m = makeMachine();
    auto& b = m.bus();
    for (const std::uint32_t addr :
         {0x020000u, 0x3FFFFEu, 0x400000u, 0x7FFFFEu, 0x800000u, 0x9FFFFEu, 0xB00000u, 0xBFFFFEu}) {
        INFO("address " << addr);
        CHECK(b.pageKind(addr) == bus::PageKind::Unmapped);
        CHECK(b.read16(addr) == m.cpu().state().irc);  // open bus: last prefetched word
    }
    CHECK(issues(m, bus::BusIssue::Unmapped) == 8);
    b.read8(0xA12001);  // Sega CD registers inside the I/O page
    CHECK(issues(m, bus::BusIssue::Unmapped) == 9);
    b.write8(0x400001, 0x12);
    CHECK(issues(m, bus::BusIssue::Unmapped) == 10);
    CHECK(b.issues().back().write);
    CHECK(b.issues().back().addr == 0x400001);
}

TEST_CASE("Work RAM is 64 KB mirrored over $E00000-$FFFFFF", "[bus]") {
    Machine m = makeMachine();
    auto& b = m.bus();
    b.write16(0xFF0000, 0xBEEF);
    CHECK(b.read16(0xE00000) == 0xBEEF);
    CHECK(b.read16(0xEF0000) == 0xBEEF);
    b.write8(0xE0FFFF, 0x5A);
    CHECK(b.read8(0xFFFFFF) == 0x5A);
    CHECK(b.fastRead[0xE0] == b.fastRead[0xFF]);
    CHECK(b.fastWrite[0xE0] == b.fastWrite[0xFF]);
    CHECK(m.bus().state().ram[0] == 0xBE);
}

TEST_CASE("Unmapped writes are logged with the PC of the writing instruction", "[bus]") {
    // $200: move.w #$1234,($400000).l
    Machine m(romWithCode({0x33FC, 0x1234, 0x0040, 0x0000}), cart::kRegionAmericas);
    std::vector<AccessKind> kinds;
    Hooks hooks;
    hooks.ctx = &kinds;
    hooks.onAccess = [](void* ctx, CpuId, AccessKind kind, std::uint32_t addr, std::uint8_t, std::uint32_t,
                        std::uint32_t pc) {
        if (pc == 0x200 && (addr == 0x400000 || addr == 0x204)) {
            static_cast<std::vector<AccessKind>*>(ctx)->push_back(kind);
        }
    };
    m.setHooks(hooks);
    REQUIRE(m.step(CpuId::M68k).instructions == 1);
    REQUIRE(m.bus().issues().size() == 1);
    const auto& e = m.bus().issues()[0];
    CHECK(e.issue == bus::BusIssue::Unmapped);
    CHECK(e.addr == 0x400000);
    CHECK(e.pc == 0x200);
    CHECK(e.value == 0x1234);
    CHECK(e.write);
    // B-2: the extension-word fetch and the data write are told apart.
    CHECK(kinds == std::vector<AccessKind>{AccessKind::Fetch, AccessKind::Write});
}

TEST_CASE("Version register follows the region; TMSS off by default", "[bus][io]") {
    CHECK(makeMachine().bus().read8(0xA10001) == 0xA0);  // overseas, NTSC, no expansion, version 0
    MachineConfig jp;
    jp.region = bus::Region::Japan;
    CHECK(makeMachine(jp).bus().read8(0xA10001) == 0x20);
    MachineConfig eu;
    eu.region = bus::Region::Europe;
    Machine pal = makeMachine(eu);
    CHECK(pal.bus().read8(0xA10001) == 0xE0);
    CHECK(pal.linesPerFrame() == kLinesPal);
    MachineConfig tmss;
    tmss.tmss = true;
    CHECK(makeMachine(tmss).bus().read8(0xA10001) == 0xA1);
    // Word reads mirror the byte registers; region defaults to the header.
    CHECK(makeMachine().bus().read16(0xA10000) == 0xA0A0);
    CHECK(bus::regionFromHeader(cart::kRegionJapan) == bus::Region::Japan);
    CHECK(bus::regionFromHeader(cart::kRegionJapan | cart::kRegionAmericas) == bus::Region::Americas);
    CHECK(bus::regionFromHeader(cart::kRegionEurope) == bus::Region::Europe);
}

TEST_CASE("Idle controllers read $7F with TH high and the 3-button TH-low pattern", "[bus][io]") {
    Machine m = makeMachine();
    auto& b = m.bus();
    CHECK(b.read8(0xA10003) == 0x7F);  // TH is an input, pulled high
    b.write8(0xA10009, 0x40);          // TH = output
    b.write8(0xA10003, 0x40);
    CHECK(b.read8(0xA10003) == 0x7F);
    b.write8(0xA10003, 0x00);
    CHECK(b.read8(0xA10003) == 0x33);  // 0 S A 0 0 D U: bits 3-2 low identify a pad
    CHECK(b.read8(0xA10009) == 0x40);
    CHECK(b.read8(0xA10005) == 0x7F);  // port 2
    CHECK(b.read8(0xA10007) == 0x7F);  // port 3 (EXT): nothing connected
    m.io().state().buttons[0] = bus::kPadStart | bus::kPadRight;
    CHECK(b.read8(0xA10003) == 0x13);
    b.write8(0xA10003, 0x40);
    CHECK(b.read8(0xA10003) == 0x77);
}

TEST_CASE("TMSS model locks the VDP until 'SEGA' is written to $A14000", "[bus][io]") {
    MachineConfig config;
    config.tmss = true;
    Machine m = makeMachine(config);
    auto& b = m.bus();
    CHECK(b.vdpLocked());
    b.read16(0xC00004);
    CHECK(issues(m, bus::BusIssue::VdpLocked) == 1);
    b.write16(0xA14000, 0x5345);
    b.write16(0xA14002, 0x4741);
    CHECK_FALSE(b.vdpLocked());
    CHECK((b.read16(0xC00004) & 0x0200) != 0);  // FIFO empty
    CHECK_FALSE(makeMachine().bus().vdpLocked());
}

TEST_CASE("Z80 bus request grants access to Z80 RAM", "[bus][z80]") {
    Machine m = makeMachine();
    auto& b = m.bus();
    CHECK((b.read16(0xA11100) & 0x0100) != 0);  // not granted
    CHECK((b.read8(0xA11100) & 0x01) != 0);
    b.write8(0xA00010, 0x77);
    b.read8(0xA00010);
    CHECK(issues(m, bus::BusIssue::Z80NotOwned) == 2);
    CHECK(m.z80().state().ram[0x10] == 0);

    b.write16(0xA11100, 0x0100);
    b.write16(0xA11200, 0x0100);
    CHECK((b.read16(0xA11100) & 0x0100) == 0);
    CHECK((b.read8(0xA11100) & 0x01) == 0);
    CHECK_FALSE(m.z80().state().resetAsserted);
    b.write8(0xA00010, 0x77);
    b.write16(0xA00012, 0x8899);  // word write stores the high byte
    CHECK(m.z80().state().ram[0x10] == 0x77);
    CHECK(m.z80().state().ram[0x12] == 0x88);
    CHECK(b.read16(0xA00010) == 0x7777);  // byte in both halves
    CHECK(b.read8(0xA02010) == 0x77);     // RAM mirror at $2000
    CHECK(b.read8(0xA04000) == 0x00);     // YM2612 status: ready
    b.write8(0xA06000, 1);
    CHECK(m.z80().state().bank == 0x100);
    CHECK(issues(m, bus::BusIssue::Z80NotOwned) == 2);

    b.write8(0xA11200, 0x00);  // byte write to the even address: assert reset
    CHECK(m.z80().state().resetAsserted);
    b.write8(0xA11100, 0x00);
    CHECK((b.read8(0xA11100) & 0x01) != 0);
}

TEST_CASE("VDP ports are mirrored every 32 bytes over $C00000-$DFFFFF", "[bus][vdp]") {
    Machine m = makeMachine();
    auto& b = m.bus();
    CHECK(b.pageKind(0xC00000) == bus::PageKind::Vdp);
    CHECK(b.pageKind(0xDF0000) == bus::PageKind::Vdp);
    b.write16(0xC00024, 0x8F02);  // control-port mirror
    CHECK(m.vdp().state().regs[15] == 0x02);
    b.write8(0xC00005, 0x81);  // B-5: byte write duplicated -> $8181 = register 1 <- $81
    CHECK(m.vdp().state().regs[1] == 0x81);
    CHECK((b.read8(0xC00005) & 0x08) != 0);  // display disabled reads as V-blank
}
