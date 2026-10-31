// VDP port stub: command parsing, memories, status, HV counter, interrupts, DMA (story M02-04; spec 05).

#include "core/vdp/vdp.hpp"

#include <cstdint>

#include <catch2/catch_test_macros.hpp>

using namespace gen::vdp;

namespace {

// Two-word command for `code` (CD5-CD0) at `addr`.
void command(Vdp& v, std::uint8_t code, std::uint16_t addr) {
    v.writeControl(static_cast<std::uint16_t>(((code & 0x03) << 14) | (addr & 0x3FFF)));
    v.writeControl(static_cast<std::uint16_t>(((code & 0x3C) << 2) | (addr >> 14)));
}

constexpr std::uint8_t kVramWrite = 0x01;
constexpr std::uint8_t kCramWrite = 0x03;
constexpr std::uint8_t kVsramWrite = 0x05;
constexpr std::uint8_t kVramRead = 0x00;
constexpr std::uint8_t kCramRead = 0x08;
constexpr std::uint8_t kDma = 0x20;

Vdp makeVdp() {
    Vdp v;
    v.reset(false);
    v.writeControl(0x8F02);  // auto-increment 2
    return v;
}

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

    command(v, kCramWrite, 0x0002);
    v.writeData(0xFFFF);
    CHECK(v.state().cram[1] == 0x0EEE);
    command(v, kCramRead, 0x0002);
    CHECK(v.readData() == 0x0EEE);
    command(v, kVsramWrite, 0x004E);
    v.writeData(0x0123);
    CHECK(v.state().vsram[39] == 0x0123);
}

TEST_CASE("DMA fill, copy and 68k-to-VRAM transfers", "[vdp]") {
    Vdp v = makeVdp();
    v.writeControl(0x8114);  // DMA enabled
    // Fill at $0400 with $AB (auto-increment 1): the data word is written first, then the fill bytes.
    v.writeControl(0x8F01);
    v.writeControl(0x9310);
    v.writeControl(0x9400);
    v.writeControl(0x9780);
    command(v, static_cast<std::uint8_t>(kVramWrite | kDma), 0x0400);
    CHECK(v.state().fillPending);
    v.writeData(0xAB00);
    CHECK_FALSE(v.state().fillPending);
    for (int i = 0x0402; i < 0x0410; ++i) {
        CHECK(v.state().vram[static_cast<std::size_t>(i)] == 0xAB);
    }
    CHECK(v.state().regs[19] == 0);

    // Copy those 16 bytes to $0800.
    v.writeControl(0x9310);
    v.writeControl(0x9500);
    v.writeControl(0x9604);
    v.writeControl(0x97C0);
    command(v, static_cast<std::uint8_t>(kVramWrite | kDma), 0x0800);  // copy uses code 0x30 on hardware
    CHECK(v.state().vram[0x0802] == 0xAB);
    CHECK(v.state().vram[0x080F] == 0xAB);

    // 68k memory -> VRAM: 4 words from $FF0000.
    struct Src {
        static std::uint16_t read(void*, std::uint32_t addr) { return static_cast<std::uint16_t>(addr); }
    };
    v.setDmaReader(&Src::read, nullptr);
    v.writeControl(0x8F02);
    v.writeControl(0x9304);
    v.writeControl(0x9400);
    v.writeControl(0x9500);  // source bits 1-8
    v.writeControl(0x9680);  // source bits 9-16: $FF0000 >> 9 = $7F80
    v.writeControl(0x977F);
    command(v, static_cast<std::uint8_t>(kVramWrite | kDma), 0x0C00);
    CHECK(v.state().vram[0x0C00] == 0x00);
    CHECK(v.state().vram[0x0C03] == 0x02);
    CHECK(v.state().vram[0x0C07] == 0x06);
    CHECK(v.state().regs[21] == 0x04);  // source advanced by 4 words
}

TEST_CASE("Status V-blank / H-blank bits and the HV counter", "[vdp]") {
    Vdp v = makeVdp();
    v.writeControl(0x8144);  // display on, V28
    v.writeControl(0x8C81);  // H40
    v.beginLine(0);
    CHECK((v.readStatus(0) & 0x0208) == 0x0200);  // FIFO empty, not in V-blank
    CHECK((v.readStatus(3300) & 0x0004) != 0);    // late in the line: H-blank
    CHECK((v.readStatus(1000) & 0x0004) == 0);
    v.beginLine(100);
    CHECK((v.readHvCounter(0) >> 8) == 100);
    v.beginLine(224);
    CHECK((v.readStatus(0) & 0x0008) != 0);
    v.beginLine(0xEB);
    CHECK((v.readHvCounter(0) >> 8) == 0xE5);  // NTSC V28 jump
    v.beginLine(261);
    CHECK((v.readHvCounter(0) >> 8) == 0xFF);
    CHECK((v.readHvCounter(3419) & 0xFF) >= 0xE4);  // H40 jump near the end of the line
    v.writeControl(0x8C00);                         // H32
    CHECK((v.readHvCounter(3419) & 0xFF) >= 0xE9);
    CHECK((v.readHvCounter(1000) & 0xFF) == 50);
}

TEST_CASE("V-int at line 224 and the H-int counter", "[vdp]") {
    Vdp v = makeVdp();
    v.writeControl(0x8164);  // display + V-int enabled
    v.writeControl(0x8A02);  // H-int every 3 lines
    v.beginLine(261);        // V-blank line: reload
    for (int line = 0; line < 224; ++line) {
        v.beginLine(line);
        CHECK(v.irqLevel() == 0);  // H-int disabled; V-int not yet
    }
    CHECK(v.state().hintPending);
    v.beginLine(224);
    CHECK(v.state().vintPending);
    CHECK((v.readStatus(0) & 0x0080) != 0);
    CHECK(v.irqLevel() == 6);
    v.acknowledge(6);
    CHECK(v.irqLevel() == 0);

    Vdp h = makeVdp();
    h.writeControl(0x8014);  // H-int enabled
    h.writeControl(0x8A02);
    h.beginLine(261);
    h.beginLine(0);
    h.beginLine(1);
    CHECK(h.irqLevel() == 0);
    h.beginLine(2);  // third active line: counter underflows
    CHECK(h.irqLevel() == 4);
    h.acknowledge(4);
    CHECK(h.irqLevel() == 0);
    h.beginLine(3);
    h.beginLine(4);
    h.beginLine(5);
    CHECK(h.irqLevel() == 4);
}
