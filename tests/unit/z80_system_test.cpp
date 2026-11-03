// Z80 system integration (M04-07): BUSREQ / RESET semantics, execution in the scheduler, the bank register
// and 68k window, the V-blank /INT and the sound-port register log.

#include <cstdint>
#include <initializer_list>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "core/cart/rom.hpp"
#include "core/sched/machine.hpp"
#include "support/synthetic_rom.hpp"

using namespace gen;

namespace {

Machine makeMachine() {
    return Machine(test::makeSyntheticRom(), cart::kRegionAmericas);
}

// Loads `code` at Z80 $0000 the way games do (BUSREQ, release RESET, copy, RESET, release BUSREQ), then
// releases RESET so the Z80 starts.
void loadZ80(Machine& m, std::initializer_list<std::uint8_t> code, std::uint16_t at = 0) {
    auto& b = m.bus();
    b.write16(0xA11100, 0x0100);
    b.write16(0xA11200, 0x0100);
    for (const std::uint8_t byte : code) {
        b.write8(0xA00000 + at++, byte);
    }
}

void startZ80(Machine& m) {
    auto& b = m.bus();
    b.write16(0xA11200, 0x0000);  // reset the Z80 (PC = 0)
    b.write16(0xA11100, 0x0000);  // give the bus back
    b.write16(0xA11200, 0x0100);  // run
}

}  // namespace

TEST_CASE("BUSREQ is not granted while the Z80 is held in reset", "[z80][bus]") {
    Machine m = makeMachine();
    auto& b = m.bus();
    b.write16(0xA11100, 0x0100);  // request, but RESET is still asserted from power on
    CHECK(m.z80().state().resetAsserted);
    CHECK((b.read16(0xA11100) & 0x0100) != 0);  // not granted
    b.write8(0xA00010, 0x55);
    CHECK(m.bus().issueCount(bus::BusIssue::Z80NotOwned) == 1);
    CHECK(m.z80().state().ram[0x10] == 0);
    b.write16(0xA11200, 0x0100);  // release reset: now granted
    CHECK((b.read16(0xA11100) & 0x0100) == 0);
    b.write8(0xA00010, 0x55);
    CHECK(m.z80().state().ram[0x10] == 0x55);
}

TEST_CASE("The Z80 runs only while neither BUSREQ nor RESET is asserted", "[z80][sched]") {
    Machine m = makeMachine();
    // ld a,$42 ; ld ($1000),a ; halt
    loadZ80(m, {0x3E, 0x42, 0x32, 0x00, 0x10, 0x76});
    m.run(RunLimit::frames(1));
    CHECK(m.z80().state().ram[0x1000] == 0);  // bus granted: the Z80 is stopped
    startZ80(m);
    m.run(RunLimit::frames(1));
    CHECK(m.z80().state().ram[0x1000] == 0x42);
    CHECK(m.z80Cpu().state().halted);
    CHECK(m.z80Cpu().state().pc == 0x0006);
    // Asserting RESET resets the CPU.
    m.bus().write16(0xA11200, 0x0000);
    CHECK(m.z80Cpu().state().pc == 0);
    CHECK_FALSE(m.z80Cpu().state().halted);
    CHECK(m.z80Mclk() >= m.lineStartMclk());
}

TEST_CASE("Bank register serial writes select the 68k window", "[z80][bank]") {
    Machine m = makeMachine();
    m.bus().state().ram[0x1234] = 0xA5;  // 68k $FF1234
    m.bus().state().ram[0x1235] = 0x00;
    // Bank $1FE = 68k $FF0000: write bits 15..23 = 0,1,1,1,1,1,1,1,1 (LSB first) to $6000.
    // ld hl,$6000 ; ld (hl),0 ; ld (hl),1 x8 ; ld a,($9234) ; ld ($1000),a ; ld a,$5A ; ld ($9235),a ; halt
    loadZ80(m, {0x21, 0x00, 0x60, 0x36, 0x00, 0x36, 0x01, 0x36, 0x01, 0x36, 0x01,
                0x36, 0x01, 0x36, 0x01, 0x36, 0x01, 0x36, 0x01, 0x36, 0x01, 0x3A,
                0x34, 0x92, 0x32, 0x00, 0x10, 0x3E, 0x5A, 0x32, 0x35, 0x92, 0x76});
    struct Ctx {
        std::uint32_t lastM68k = 0;
        int count = 0;
    } ctx;
    Hooks hooks;
    hooks.ctx = &ctx;
    hooks.onZ80Window = [](void* c, AccessKind, std::uint16_t, std::uint32_t m68k, std::uint8_t,
                           std::uint16_t) {
        auto& x = *static_cast<Ctx*>(c);
        x.lastM68k = m68k;
        ++x.count;
    };
    m.setHooks(hooks);
    startZ80(m);
    m.run(RunLimit::frames(1));
    CHECK(m.z80().state().bank == 0x1FE);
    CHECK(m.z80().state().ram[0x1000] == 0xA5);
    CHECK(m.bus().state().ram[0x1235] == 0x5A);  // Z80 write through the window into 68k RAM
    CHECK(ctx.count == 2);
    CHECK(ctx.lastM68k == 0xFF1235);  // CZ-6: translated 68k address
}

TEST_CASE("V-blank asserts the Z80 /INT once per frame (IM 1)", "[z80][irq]") {
    Machine m = makeMachine();
    // 0000: di ; im 1 ; ld sp,$2000 ; ei ; jr $
    // 0038: ld hl,$1000 ; inc (hl) ; ld b,40 ; djnz $ ; ei ; reti   (the delay outlasts the /INT line)
    loadZ80(m, {0xF3, 0xED, 0x56, 0x31, 0x00, 0x20, 0xFB, 0x18, 0xFE});
    loadZ80(m, {0x21, 0x00, 0x10, 0x34, 0x06, 0x28, 0x10, 0xFE, 0xFB, 0xED, 0x4D}, 0x38);
    startZ80(m);
    m.run(RunLimit::frames(5));
    const int ints = m.z80().state().ram[0x1000];
    CHECK((ints == 4 || ints == 5));  // one per V-blank (the first frame may start after its V-blank)
}

TEST_CASE("Sound-port writes reach the register log with CPU and position", "[z80][audio]") {
    Machine m = makeMachine();
    // ld a,$28 ; ld ($4000),a ; ld a,$F0 ; ld ($4001),a ; ld a,$9F ; ld ($7F11),a ; halt
    loadZ80(m,
            {0x3E, 0x28, 0x32, 0x00, 0x40, 0x3E, 0xF0, 0x32, 0x01, 0x40, 0x3E, 0x9F, 0x32, 0x11, 0x7F, 0x76});
    startZ80(m);
    m.bus().write8(0xC00011, 0xBF);  // a 68k PSG write
    m.run(RunLimit::frames(1));
    const auto& log = m.z80().log();
    REQUIRE(log.size() == 3);
    CHECK(log.at(0).cpu == CpuId::M68k);
    CHECK(log.at(0).chip == audio::Chip::Psg);
    CHECK(log.at(0).reg == 3);  // latch byte $BF: channel 1 volume
    CHECK(log.at(1).cpu == CpuId::Z80);
    CHECK(log.at(1).chip == audio::Chip::Ym2612);
    CHECK(log.at(1).reg == 0x28);
    CHECK(log.at(1).value == 0xF0);
    CHECK(log.at(2).chip == audio::Chip::Psg);
    CHECK(m.z80().state().ymRegs[0][0x28] == 0xF0);
    CHECK(log.toCsv().starts_with("frame,line,cpu,pc,chip,port,reg,value\n"));
}
