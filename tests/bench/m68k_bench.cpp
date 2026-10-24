// 68000 throughput microbenchmark (story M01-11): runs a mix of typical Sonic-style code (block copies,
// table lookups, arithmetic, branches, subroutine calls) and prints instructions per second.
//
//   gen_m68k_bench [millions-of-instructions] [--virtual]   (default 50; --virtual disables fast pages)

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string_view>
#include <vector>

#include "core/m68k/m68k.hpp"

namespace {

class Bus final : public gen::m68k::Bus68k {
public:
    std::vector<std::uint8_t> mem = std::vector<std::uint8_t>(0x1000000, 0);
    explicit Bus(bool direct) {
        for (std::size_t p = 0; direct && p < 256; ++p) {
            fastRead[p] = mem.data() + (p << 16);
            fastWrite[p] = mem.data() + (p << 16);
        }
    }
    std::uint8_t read8(std::uint32_t a) override { return mem[a]; }
    std::uint16_t read16(std::uint32_t a) override {
        return static_cast<std::uint16_t>((mem[a] << 8) | mem[a + 1]);
    }
    void write8(std::uint32_t a, std::uint8_t v) override { mem[a] = v; }
    void write16(std::uint32_t a, std::uint16_t v) override {
        mem[a] = static_cast<std::uint8_t>(v >> 8);
        mem[a + 1] = static_cast<std::uint8_t>(v);
    }
    void put(std::uint32_t& a, std::uint16_t w) {
        write16(a, w);
        a += 2;
    }
};

}  // namespace

int main(int argc, char** argv) {
    const long long millions = argc > 1 ? std::atoll(argv[1]) : 50;
    const bool direct = !(argc > 2 && std::string_view(argv[2]) == "--virtual");  // --virtual: no fast pages
    Bus bus(direct);
    // Vectors: SSP $FFFE00, PC $000200.
    std::uint32_t a = 0;
    for (const std::uint16_t w :
         {std::uint16_t{0x00FF}, std::uint16_t{0xFE00}, std::uint16_t{0}, std::uint16_t{0x0200}}) {
        bus.put(a, w);
    }
    a = 0x200;
    const std::uint16_t program[] = {
        0x41F9, 0x00FF, 0x0000,  // loop: lea     $FF0000,a0
        0x43F9, 0x00FF, 0x8000,  //       lea     $FF8000,a1
        0x303C, 0x003F,          //       move.w  #63,d0
        0x22D8,                  // copy: move.l  (a0)+,(a1)+
        0xD282,                  //       add.l   d2,d1
        0x51C8, 0xFFFA,          //       dbf     d0,copy
        0x7407,                  //       moveq   #7,d2
        0x3632, 0x2000,          //       move.w  0(a2,d2.w),d3
        0xC6FC, 0x0013,          //       mulu.w  #19,d3
        0x6100, 0x0006,          //       bsr.w   sub
        0x4EF8, 0x0200,          //       jmp     loop.w
        0xE34B,                  // sub:  lsl.w   #1,d3
        0x0C43, 0x1000,          //       cmpi.w  #$1000,d3
        0x6302,                  //       bls.s   +2
        0x4243,                  //       clr.w   d3
        0x4E75,                  //       rts
    };
    for (const std::uint16_t w : program) {
        bus.put(a, w);
    }

    gen::m68k::M68k cpu(bus);
    cpu.reset();
    cpu.state().a[2] = 0xFF4000;
    const long long target = millions * 1000000;
    long long instructions = 0;
    long long cycles = 0;
    const auto start = std::chrono::steady_clock::now();
    while (instructions < target) {
        cycles += cpu.step();
        ++instructions;
    }
    const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    const double mips = static_cast<double>(instructions) / seconds / 1e6;
    std::printf("%lld instructions, %lld cycles in %.3f s: %.1f MIPS (%.1fx a 7.67 MHz 68000)\n",
                instructions, cycles, seconds, mips, static_cast<double>(cycles) / seconds / 7.67e6);
    return 0;
}
