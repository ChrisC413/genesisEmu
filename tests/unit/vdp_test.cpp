// VDP ports, memories, DMA, interrupts and counters (stories M02-04, M03-01..M03-03; spec 05).

#include "core/vdp/vdp.hpp"

#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

using namespace gen::vdp;

namespace {

// Two-word command for `code` (CD5-CD0) at `addr`.
void command(Vdp& v, std::uint8_t code, std::uint16_t addr) {
    v.writeControl(static_cast<std::uint16_t>(((code & 0x03) << 14) | (addr & 0x3FFF)));
    v.writeControl(static_cast<std::uint16_t>(((code & 0x3C) << 2) | (addr >> 14)));
}

void reg(Vdp& v, int r, int value) {
    v.writeControl(static_cast<std::uint16_t>(0x8000 | (r << 8) | value));
}

constexpr std::uint8_t kVramWrite = 0x01;
constexpr std::uint8_t kCramWrite = 0x03;
constexpr std::uint8_t kVsramWrite = 0x05;
constexpr std::uint8_t kVramRead = 0x00;
constexpr std::uint8_t kCramRead = 0x08;
constexpr std::uint8_t kVsramRead = 0x04;
constexpr std::uint8_t kVram8Read = 0x0C;
constexpr std::uint8_t kDma = 0x20;
constexpr int kMclkPerLine = 3420;

Vdp makeVdp() {
    Vdp v;
    v.reset(false);
    reg(v, 15, 2);  // auto-increment 2
    return v;
}

// Runs a whole 68k DMA; returns the master clocks it took starting at the beginning of line `line`.
int runWholeDma(Vdp& v, int line) {
    int total = 0;
    while (v.dmaFreezesCpu()) {
        v.beginLine(line);
        total += v.runDma(kMclkPerLine);
        v.endLine(line);
        line = (line + 1) % 262;
    }
    return total;
}

struct Src {
    static std::uint16_t read(void*, std::uint32_t addr) { return static_cast<std::uint16_t>(addr); }
};

}  // namespace

TEST_CASE("Register writes and the two-word command latch", "[vdp]") {
    Vdp v = makeVdp();
    v.writeControl(0x8174);
    CHECK(v.state().regs[1] == 0x74);
    v.writeControl(0x9701);
    CHECK(v.state().regs[23] == 0x01);
    v.writeControl(0x9801);  // register 24+ do not exist
    CHECK(v.state().regs[23] == 0x01);

    v.writeControl(0x4123);  // first word: VRAM write, A13-A0 = $0123
    CHECK(v.state().pending);
    v.writeControl(0x0002);  // second word: A15-A14 = 2
    CHECK_FALSE(v.state().pending);
    CHECK(v.state().address == 0x8123);
    CHECK(v.state().code == kVramWrite);

    v.writeControl(0x4000);
    v.readStatus(0);  // a status read cancels the pending second word
    CHECK_FALSE(v.state().pending);
    v.writeControl(0x8F04);  // so this is a register write again
    CHECK(v.state().regs[15] == 0x04);

    v.writeControl(0x4000);
    v.writeData(0);  // so do data-port writes ...
    CHECK_FALSE(v.state().pending);
    v.writeControl(0x4000);
    v.readData();  // ... and reads
    CHECK_FALSE(v.state().pending);
}

TEST_CASE("Command latch edge cases: A15-A14 latch and register writes reloading the code", "[vdp]") {
    Vdp v = makeVdp();
    command(v, kVramWrite, 0xC010);
    // A lone first word keeps A15-A14 from the last second word.
    v.writeControl(0x4020);
    CHECK(v.state().pending);
    CHECK(v.state().address == 0xC020);
    v.readStatus(0);
    // A register write loads CD1-CD0 = 2 (no valid target) and the address bits too: data written next
    // is dropped instead of landing in VRAM.
    command(v, kVramWrite, 0x0100);
    reg(v, 15, 2);
    CHECK_FALSE(v.state().pending);
    CHECK((v.state().code & 0x03) == 0x02);
    v.writeData(0x1234);
    CHECK(v.state().vram[0x0100] == 0);
    CHECK(v.state().vram[0x0F02] == 0);
    // The DMA bit is ignored when DMA is disabled in register 1.
    command(v, static_cast<std::uint8_t>(kVramWrite | kDma), 0x0200);
    CHECK_FALSE(v.dmaFreezesCpu());
    CHECK_FALSE(v.state().fillPending);
}

TEST_CASE("VRAM, CRAM and VSRAM writes with auto-increment", "[vdp]") {
    Vdp v = makeVdp();
    command(v, kVramWrite, 0x1000);
    v.writeData(0x1234);
    v.writeData(0x5678);
    CHECK(v.state().vram[0x1000] == 0x12);
    CHECK(v.state().vram[0x1003] == 0x78);
    command(v, kVramWrite, 0x2001);  // odd address: bytes swapped into the word
    v.writeData(0xAABB);
    CHECK(v.state().vram[0x2000] == 0xBB);
    CHECK(v.state().vram[0x2001] == 0xAA);
    command(v, kVramRead, 0x1000);
    CHECK(v.readData() == 0x1234);
    CHECK(v.readData() == 0x5678);
    command(v, kVram8Read, 0x1000);  // undocumented 8-bit read: the byte at A ^ 1 in the low half
    CHECK((v.readData() & 0xFF) == 0x34);

    command(v, kCramWrite, 0x0002);
    v.writeData(0xFFFF);
    CHECK(v.state().cram[1] == 0x0EEE);
    command(v, kCramRead, 0x0002);
    CHECK((v.readData() & 0x0EEE) == 0x0EEE);
    command(v, kVsramWrite, 0x004E);
    v.writeData(0x0123);
    CHECK(v.state().vsram[39] == 0x0123);
    command(v, kVsramRead, 0x004E);
    CHECK((v.readData() & 0x07FF) == 0x0123);

    // Writes with a read code (or an invalid code) are dropped but still advance the address.
    command(v, kVramRead, 0x3000);
    v.writeData(0xBEEF);
    CHECK(v.state().vram[0x3000] == 0);
    CHECK(v.state().address == 0x3002);
    command(v, 0x02, 0x3000);
    v.writeData(0xBEEF);
    CHECK(v.state().vram[0x3000] == 0);
}

TEST_CASE("CRAM and VSRAM addresses wrap; VSRAM has 40 entries", "[vdp]") {
    Vdp v = makeVdp();
    command(v, kCramWrite, 0x007E);
    v.writeData(0x0002);  // entry 63
    v.writeData(0x0004);  // wraps to entry 0
    CHECK(v.state().cram[63] == 0x0002);
    CHECK(v.state().cram[0] == 0x0004);
    command(v, kCramWrite, 0x0082);  // A7+ ignored: entry 1
    v.writeData(0x0006);
    CHECK(v.state().cram[1] == 0x0006);

    command(v, kVsramWrite, 0x0050);  // entry 40: not backed, dropped
    v.writeData(0x0111);
    for (const auto w : v.state().vsram) {
        CHECK(w == 0);
    }
    command(v, kVsramWrite, 0x0080);  // wraps to entry 0
    v.writeData(0x0FFF);
    CHECK(v.state().vsram[0] == 0x07FF);
}

TEST_CASE("Status register: FIFO approximation, V-blank / H-blank, DMA busy, flags clear on read", "[vdp]") {
    Vdp v = makeVdp();
    reg(v, 1, 0x44);   // display on, V28
    reg(v, 12, 0x81);  // H40
    v.beginLine(10);
    command(v, kVramWrite, 0);
    CHECK((v.readStatus(0) & 0x0300) == 0x0200);  // empty
    for (int i = 0; i < 4; ++i) {
        v.writeData(0);
    }
    CHECK((v.readStatus(0) & 0x0300) == 0x0100);  // full
    v.endLine(10);
    v.beginLine(11);
    CHECK((v.readStatus(0) & 0x0300) == 0x0200);  // drained by the next line
    v.beginLine(230);
    v.writeData(0);  // in V-blank writes drain immediately
    CHECK((v.readStatus(0) & 0x0200) != 0);

    v.state().spriteCollision = true;
    v.state().spriteOverflow = true;
    CHECK((v.readStatus(0) & 0x0060) == 0x0060);
    CHECK((v.readStatus(0) & 0x0060) == 0);

    Vdp p;
    p.reset(true);
    CHECK((p.readStatus(0) & 0x0001) != 0);  // PAL
}

TEST_CASE("Sprite attribute cache follows VRAM writes inside the sprite table only", "[vdp]") {
    Vdp v = makeVdp();
    reg(v, 12, 0x81);                // H40
    reg(v, 5, 0x78);                 // SAT at $F000
    command(v, kVramWrite, 0xF008);  // sprite 1: Y, size/link, name, X
    v.writeData(0x0123);
    v.writeData(0x0502);
    v.writeData(0x4444);
    v.writeData(0x0155);
    const auto& c = v.state().satCache;
    CHECK(c[4] == 0x01);
    CHECK(c[5] == 0x23);
    CHECK(c[6] == 0x05);
    CHECK(c[7] == 0x02);
    // Moving the table does not reload the cache (hardware behavior some games rely on).
    reg(v, 5, 0x70);
    CHECK(c[5] == 0x23);
    command(v, kVramWrite, 0xF000 + 80 * 8);  // just past the table
    v.writeData(0x7777);
    reg(v, 5, 0x78);
    CHECK(c[0] == 0);
}

TEST_CASE("DMA fill and copy; DMA busy while they run", "[vdp][dma]") {
    Vdp v = makeVdp();
    std::vector<gen::DmaEvent> events;
    gen::Hooks hooks;
    hooks.ctx = &events;
    hooks.onDma = [](void* ctx, const gen::DmaEvent& e) {
        static_cast<std::vector<gen::DmaEvent>*>(ctx)->push_back(e);
    };
    v.setHooks(&hooks);
    reg(v, 1, 0x54);  // display + DMA enabled
    // Fill at $0400 with $AB (auto-increment 1): the data word is written first, then the fill bytes.
    reg(v, 15, 1);
    reg(v, 19, 0x10);
    reg(v, 20, 0x00);
    reg(v, 23, 0x80);
    command(v, static_cast<std::uint8_t>(kVramWrite | kDma), 0x0400);
    CHECK(v.state().fillPending);
    CHECK((v.readStatus(0) & 0x0002) != 0);
    v.writeData(0xAB00);
    CHECK_FALSE(v.state().fillPending);
    for (int i = 0x0402; i < 0x0410; ++i) {
        CHECK(v.state().vram[static_cast<std::size_t>(i)] == 0xAB);
    }
    CHECK(v.state().regs[19] == 0);
    REQUIRE(events.size() == 1);
    CHECK(events[0].kind == DmaKind::Fill);
    CHECK(events[0].dest == 0x0400);
    CHECK(events[0].length == 0x10);
    v.beginLine(0);
    CHECK((v.readStatus(0) & 0x0002) != 0);  // busy for about one active line's bandwidth
    v.endLine(0);
    v.beginLine(1);
    CHECK((v.readStatus(0) & 0x0002) == 0);

    // Copy those 16 bytes to $0800.
    reg(v, 19, 0x10);
    reg(v, 21, 0x00);
    reg(v, 22, 0x04);
    reg(v, 23, 0xC0);
    command(v, static_cast<std::uint8_t>(0x10 | kDma), 0x0800);  // copy uses code 0x30
    CHECK(v.state().vram[0x0802] == 0xAB);
    CHECK(v.state().vram[0x080F] == 0xAB);
    CHECK(v.state().regs[21] == 0x10);  // source advanced
    REQUIRE(events.size() == 2);
    CHECK(events[1].kind == DmaKind::Copy);
    CHECK(events[1].source == 0x0400);
}

TEST_CASE("68k-to-VDP DMA runs at the per-line bandwidth and updates the registers", "[vdp][dma]") {
    Vdp v = makeVdp();
    std::vector<gen::DmaEvent> events;
    gen::Hooks hooks;
    hooks.ctx = &events;
    hooks.onDma = [](void* ctx, const gen::DmaEvent& e) {
        static_cast<std::vector<gen::DmaEvent>*>(ctx)->push_back(e);
    };
    v.setHooks(&hooks);
    v.setDmaReader(&Src::read, nullptr);
    reg(v, 1, 0x54);   // display + DMA enabled
    reg(v, 12, 0x81);  // H40: 18 bytes per active line
    // 90 words from $FF0000 during active display: 180 bytes = 10 lines.
    reg(v, 19, 90);
    reg(v, 20, 0);
    reg(v, 21, 0x00);
    reg(v, 22, 0x80);
    reg(v, 23, 0x7F);
    v.beginLine(0);
    command(v, static_cast<std::uint8_t>(kVramWrite | kDma), 0x0C00);
    REQUIRE(v.dmaFreezesCpu());
    CHECK(v.state().vram[0x0C00] == 0);  // nothing moved until the VDP gets time
    const int mclk = runWholeDma(v, 0);
    CHECK(mclk == 10 * kMclkPerLine);
    CHECK(v.state().vram[0x0C03] == 0x02);
    CHECK(v.state().vram[0x0C00 + 89 * 2 + 1] == 178);
    CHECK(v.state().regs[19] == 0);
    CHECK(v.state().regs[20] == 0);
    CHECK(v.state().regs[21] == 90);  // source advanced by 90 words
    REQUIRE(events.size() == 1);
    CHECK(events[0].kind == DmaKind::MemoryToVdp);
    CHECK(events[0].target == DmaTarget::Vram);
    CHECK(events[0].source == 0xFF0000);
    CHECK(events[0].dest == 0x0C00);
    CHECK(events[0].length == 90);

    // In V-blank the same transfer takes under one line (205 bytes per line).
    reg(v, 19, 90);
    reg(v, 21, 0x00);
    command(v, static_cast<std::uint8_t>(kVramWrite | kDma), 0x0C00);
    const int blank = runWholeDma(v, 230);
    CHECK(blank < kMclkPerLine);
    CHECK(blank > kMclkPerLine * 3 / 4);

    // CRAM target, and the source wraps inside its 128 KB window.
    reg(v, 19, 4);
    reg(v, 21, 0xFE);
    reg(v, 22, 0xFF);
    reg(v, 23, 0x00);  // source $01FFFC
    command(v, static_cast<std::uint8_t>(kCramWrite | kDma), 0x0000);
    runWholeDma(v, 230);
    CHECK(v.state().cram[0] == (0x1FFFC & 0x0EEE));
    CHECK(v.state().cram[1] == (0x1FFFE & 0x0EEE));
    CHECK(v.state().cram[2] == 0);  // $020000 wrapped to $000000
    CHECK(v.state().cram[3] == 2);
    CHECK(events.back().target == DmaTarget::Cram);
}

TEST_CASE("HV counter: V28 / V30 jumps, H40 / H32 jumps, interlace", "[vdp][hv]") {
    Vdp v = makeVdp();
    reg(v, 1, 0x44);
    reg(v, 12, 0x81);
    v.beginLine(0);
    CHECK((v.readStatus(3300) & 0x0004) != 0);  // late in the line: H-blank
    CHECK((v.readStatus(1000) & 0x0004) == 0);
    v.beginLine(100);
    CHECK((v.readHvCounter(0) >> 8) == 100);
    v.beginLine(224);
    CHECK((v.readStatus(0) & 0x0008) != 0);
    v.beginLine(0xEA);
    CHECK((v.readHvCounter(0) >> 8) == 0xEA);
    v.beginLine(0xEB);
    CHECK((v.readHvCounter(0) >> 8) == 0xE5);  // NTSC V28 jump
    v.beginLine(261);
    CHECK((v.readHvCounter(0) >> 8) == 0xFF);
    CHECK((v.readHvCounter(3419) & 0xFF) >= 0xE4);  // H40 jump near the end of the line
    CHECK((v.readHvCounter(1000) & 0xFF) == 61);
    reg(v, 12, 0x00);  // H32
    CHECK((v.readHvCounter(3419) & 0xFF) >= 0xE9);
    CHECK((v.readHvCounter(1000) & 0xFF) == 50);

    reg(v, 12, 0x06);  // interlace mode 2: doubled line number, bit 0 = bit 8
    v.beginLine(100);
    CHECK((v.readHvCounter(0) >> 8) == 200);
    reg(v, 12, 0x02);  // interlace mode 1
    v.beginLine(0x101 - 0x100 + 0xE0);
    CHECK((v.readHvCounter(0) >> 8) == 0xE0);

    Vdp p;
    p.reset(true);
    reg(p, 1, 0x4C);  // PAL V30
    p.beginLine(0x10A);
    CHECK((p.readHvCounter(0) >> 8) == 0x0A);
    p.beginLine(0x10B);
    CHECK((p.readHvCounter(0) >> 8) == 0xD2);
    reg(p, 1, 0x44);  // PAL V28
    p.beginLine(0x103);
    CHECK((p.readHvCounter(0) >> 8) == 0xCA);
}

TEST_CASE("V-int at the first V-blank line, Z80 /INT for one line, acknowledge", "[vdp][irq]") {
    Vdp v = makeVdp();
    reg(v, 1, 0x64);  // display + V-int enabled
    for (int line = 0; line < 224; ++line) {
        v.beginLine(line);
        CHECK(v.irqLevel() == 0);
        CHECK_FALSE(v.state().z80Int);
        v.endLine(line);
    }
    v.beginLine(224);
    CHECK(v.state().vintPending);
    CHECK(v.state().z80Int);
    CHECK((v.readStatus(0) & 0x0080) != 0);
    CHECK(v.irqLevel() == 6);
    v.endLine(224);
    v.beginLine(225);
    CHECK_FALSE(v.state().z80Int);
    v.acknowledge(6);
    CHECK(v.irqLevel() == 0);
    CHECK((v.readStatus(0) & 0x0080) == 0);

    // V-int pending while disabled fires as soon as it is enabled.
    Vdp d = makeVdp();
    reg(d, 1, 0x44);
    d.beginLine(224);
    CHECK(d.irqLevel() == 0);
    reg(d, 1, 0x64);
    CHECK(d.irqLevel() == 6);
}

TEST_CASE("H-int counter: decremented after each active line, reloaded in V-blank", "[vdp][irq]") {
    Vdp h = makeVdp();
    reg(h, 0, 0x14);  // H-int enabled
    reg(h, 1, 0x44);
    reg(h, 10, 2);  // every 3 lines
    h.beginLine(261);
    h.endLine(261);  // V-blank line: reload
    std::vector<int> hints;
    for (int line = 0; line < 262; ++line) {
        h.beginLine(line);
        h.endLine(line);
        if (h.irqLevel() == 4) {
            hints.push_back(line);
            h.acknowledge(4);
        }
    }
    // Raised at the end of lines 2, 5, 8, ...: the handler runs during the next line.
    REQUIRE(hints.size() == 75);
    CHECK(hints[0] == 2);
    CHECK(hints[1] == 5);
    CHECK(hints.back() == 224);  // the first V-blank line still counts

    // Register 10 = 0: every line; the reload value is taken in V-blank.
    Vdp e = makeVdp();
    reg(e, 0, 0x14);
    reg(e, 1, 0x44);
    reg(e, 10, 0);
    e.beginLine(250);
    e.endLine(250);
    int count = 0;
    for (int line = 0; line < 224; ++line) {
        e.beginLine(line);
        e.endLine(line);
        count += e.irqLevel() == 4 ? 1 : 0;
        e.acknowledge(4);
    }
    CHECK(count == 224);
}
