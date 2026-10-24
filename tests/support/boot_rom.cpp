// A tiny hand-assembled boot program in the style of a Genesis game: TMSS check, Z80 bus request and
// RAM upload, controller port init, VDP setup, a VRAM / CRAM upload, then a main loop that waits for a
// RAM flag the V-int handler clears. Original code written for these tests; no commercial data.

#include <initializer_list>

#include "core/cart/rom.hpp"
#include "synthetic_rom.hpp"

namespace gen::test {

namespace {

class Emitter {
public:
    explicit Emitter(std::vector<std::uint8_t>& rom, std::uint32_t at) : rom_(rom), pc_(at) {}
    void w(std::initializer_list<std::uint16_t> words) {
        for (const std::uint16_t v : words) {
            rom_[pc_++] = static_cast<std::uint8_t>(v >> 8);
            rom_[pc_++] = static_cast<std::uint8_t>(v);
        }
    }
    // Bcc.s with an 8-bit displacement to `target`.
    void branch(std::uint16_t op, std::uint32_t target) {
        const auto disp = static_cast<std::int32_t>(target) - static_cast<std::int32_t>(pc_ + 2);
        w({static_cast<std::uint16_t>(op | (static_cast<std::uint32_t>(disp) & 0xFF))});
    }
    std::uint32_t here() const { return pc_; }

private:
    std::vector<std::uint8_t>& rom_;
    std::uint32_t pc_;
};

void putLong(std::vector<std::uint8_t>& rom, std::uint32_t at, std::uint32_t v) {
    for (std::uint32_t i = 0; i < 4; ++i) {
        rom[at + i] = static_cast<std::uint8_t>(v >> (24 - 8 * i));
    }
}

}  // namespace

std::vector<std::uint8_t> makeBootTestRom() {
    SyntheticRomOptions options;
    options.domesticTitle = "BOOT TEST ROM";
    options.overseasTitle = "BOOT TEST ROM";
    options.regionCodes = "U";
    std::vector<std::uint8_t> rom = makeSyntheticRom(options);

    for (std::uint32_t v = 2; v < 64; ++v) {
        putLong(rom, v * 4, kBootFaultHandler);
    }
    putLong(rom, 28 * 4, kBootHintHandler);
    putLong(rom, 30 * 4, kBootVintHandler);

    Emitter e(rom, 0x200);
    e.w({0x46FC, 0x2700});                          // move.w #$2700,sr
    e.w({0x1039, 0x00A1, 0x0001});                  // move.b ($A10001).l,d0
    e.w({0x0200, 0x000F});                          // andi.b #$0F,d0
    e.w({0x670A});                                  // beq.s (skip the TMSS write)
    e.w({0x23FC, 0x5345, 0x4741, 0x00A1, 0x4000});  // move.l #'SEGA',($A14000).l
    e.w({0x33FC, 0x0100, 0x00A1, 0x1100});          // move.w #$100,($A11100).l   Z80 bus request
    e.w({0x33FC, 0x0100, 0x00A1, 0x1200});          // move.w #$100,($A11200).l   Z80 reset released
    const std::uint32_t waitBus = e.here();
    e.w({0x0839, 0x0000, 0x00A1, 0x1100});  // btst #0,($A11100).l
    e.branch(0x6600, waitBus);              // bne.s waitBus
    e.w({0x41F9, 0x00A0, 0x0000});          // lea ($A00000).l,a0
    e.w({0x10FC, 0x00F3});                  // move.b #$F3,(a0)+   (Z80: di)
    e.w({0x10FC, 0x00C3});                  // move.b #$C3,(a0)+   (Z80: jp $0000)
    e.w({0x10FC, 0x0000});                  // move.b #0,(a0)+
    e.w({0x10FC, 0x0000});                  // move.b #0,(a0)+
    e.w({0x33FC, 0x0000, 0x00A1, 0x1200});  // move.w #0,($A11200).l     Z80 reset
    e.w({0x33FC, 0x0000, 0x00A1, 0x1100});  // move.w #0,($A11100).l     release the bus
    e.w({0x33FC, 0x0100, 0x00A1, 0x1200});  // move.w #$100,($A11200).l
    e.w({0x13FC, 0x0040, 0x00A1, 0x0009});  // move.b #$40,($A10009).l   port 1 TH = output
    e.w({0x43F9, 0x00C0, 0x0004});          // lea ($C00004).l,a1
    e.w({0x32BC, 0x8004});                  // move.w #$8004,(a1)   H-int off
    e.w({0x32BC, 0x8174});                  // move.w #$8174,(a1)   display, V-int, DMA on; V28
    e.w({0x32BC, 0x8F02});                  // move.w #$8F02,(a1)   auto-increment 2
    e.w({0x22BC, 0x4020, 0x0000});          // move.l #$40200000,(a1)   VRAM write $0020
    e.w({0x45F9, 0x00C0, 0x0000});          // lea ($C00000).l,a2
    e.w({0x720F});                          // moveq #15,d1
    const std::uint32_t tileLoop = e.here();
    e.w({0x3481});                                                         // move.w d1,(a2)
    e.w({0x51C9, static_cast<std::uint16_t>(tileLoop - (e.here() + 2))});  // dbf d1,tileLoop
    e.w({0x22BC, 0xC000, 0x0000});  // move.l #$C0000000,(a1)   CRAM write 0
    e.w({0x34BC, 0x0EEE});          // move.w #$0EEE,(a2)
    e.w({0x46FC, 0x2000});          // move.w #$2000,sr
    const std::uint32_t mainLoop = e.here();
    e.w({0x11FC, 0x0001, 0xF000});  // move.b #1,($F000).w   V-int flag
    const std::uint32_t waitVint = e.here();
    e.w({0x4A38, 0xF000});       // tst.b ($F000).w
    e.branch(0x6600, waitVint);  // bne.s waitVint
    e.w({0x5278, 0xF002});       // addq.w #1,($F002).w   main-loop frame counter
    e.branch(0x6000, mainLoop);  // bra.s mainLoop

    Emitter fault(rom, kBootFaultHandler);
    fault.w({0x60FE});  // bra.s *
    Emitter hint(rom, kBootHintHandler);
    hint.w({0x4E73});  // rte
    Emitter vint(rom, kBootVintHandler);
    vint.w({0x4238, 0xF000});                  // clr.b ($F000).w
    vint.w({0x52B8, 0xF004});                  // addq.l #1,($F004).w   V-int counter
    vint.w({0x31F9, 0x00C0, 0x0008, 0xF008});  // move.w ($C00008).l,($F008).w   HV counter
    vint.w({0x4E73});                          // rte

    const std::uint16_t sum = cart::computeChecksum(rom);
    rom[0x18E] = static_cast<std::uint8_t>(sum >> 8);
    rom[0x18F] = static_cast<std::uint8_t>(sum);
    return rom;
}

}  // namespace gen::test
