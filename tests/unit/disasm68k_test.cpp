// 68000 disassembler golden-text and consistency tests (story M01-12).

#include "analysis/disasm68k/disasm68k.hpp"

#include <cstdint>
#include <initializer_list>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "core/m68k/m68k.hpp"

using namespace gen::analysis::disasm68k;

namespace {

std::vector<std::uint8_t> words(std::initializer_list<std::uint16_t> ws) {
    std::vector<std::uint8_t> out;
    for (const std::uint16_t w : ws) {
        out.push_back(static_cast<std::uint8_t>(w >> 8));
        out.push_back(static_cast<std::uint8_t>(w));
    }
    return out;
}

std::string dis(std::initializer_list<std::uint16_t> ws, std::uint32_t address = 0x1000) {
    return disassemble(words(ws), address).text();
}

}  // namespace

TEST_CASE("Disassembler decodes exactly the opcodes the core decodes", "[disasm]") {
    int mismatches = 0;
    for (std::uint32_t op = 0; op < 65536; ++op) {
        const bool core = gen::m68k::isValidOpcode(static_cast<std::uint16_t>(op)) || op == 0x4AFC;
        if (core != isValidFirstWord(static_cast<std::uint16_t>(op)) && mismatches++ < 10) {
            UNSCOPED_INFO("opcode " << op << " core=" << core);
        }
    }
    CHECK(mismatches == 0);
}

TEST_CASE("Golden text: moves, sizes and addressing modes", "[disasm]") {
    CHECK(dis({0x3218}) == "move.w\t(a0)+,d1");
    CHECK(dis({0x203C, 0x0000, 0x0001}) == "move.l\t#$1,d0");
    CHECK(dis({0x3038, 0x8000}) == "move.w\t($FFFF8000).w,d0");
    CHECK(dis({0x3038, 0x1234}) == "move.w\t($1234).w,d0");
    CHECK(dis({0x2039, 0x00FF, 0x0000}) == "move.l\t($00FF0000).l,d0");
    CHECK(dis({0x3028, 0xFFF0}) == "move.w\t-$10(a0),d0");
    CHECK(dis({0x3230, 0x9880}) == "move.w\t-$80(a0,a1.l),d1");
    CHECK(dis({0x2F00}) == "move.l\td0,-(sp)");
    CHECK(dis({0x2040}) == "movea.l\td0,a0");
    CHECK(dis({0x70FF}) == "moveq.l\t#-$1,d0");
    CHECK(dis({0x4E71}) == "nop");
}

TEST_CASE("Golden text: MOVEM register lists", "[disasm]") {
    CHECK(dis({0x48E7, 0xFFFE}) == "movem.l\td0-d7/a0-a6,-(sp)");  // -(An) uses the reversed mask
    CHECK(dis({0x4CDF, 0x7FFF}) == "movem.l\t(sp)+,d0-d7/a0-a6");
    CHECK(dis({0x4C9F, 0x0101}) == "movem.w\t(sp)+,d0/a0");
    CHECK(dis({0x48D0, 0x0505}) == "movem.l\td0/d2/a0/a2,(a0)");
    CHECK(disassemble(words({0x48D0, 0x0000}), 0).mnemonic == "dc.w");  // empty list: not expressible in AS
}

TEST_CASE("Golden text: PC-relative operands and branches print target addresses", "[disasm]") {
    CHECK(dis({0x41FA, 0x0002}, 0x1000) == "lea.l\t$00001004(pc),a0");
    CHECK(dis({0x323B, 0x00C6}, 0x1000) == "move.w\t($00000FC8,pc,d0.w),d1");
    CHECK(dis({0x6000, 0x000E}, 0x1000) == "bra.w\t$00001010");
    CHECK(dis({0x66FE}, 0x1000) == "bne.s\t$00001000");
    CHECK(dis({0x51C8, 0xFFFC}, 0x1000) == "dbf.w\td0,$00000FFE");
    CHECK(dis({0x6001}, 0x1000) == "bra.s	$00001003");  // odd target: an address error at run time
}

TEST_CASE("Golden text: BCD, bit, shift and system instructions", "[disasm]") {
    CHECK(dis({0xC308}) == "abcd.b\t-(a0),-(a1)");
    CHECK(dis({0x8300}) == "sbcd.b\td0,d1");
    CHECK(dis({0x4800}) == "nbcd.b\td0");
    CHECK(dis({0x0800, 0x0003}) == "btst.l\t#3,d0");
    CHECK(dis({0x0310}) == "btst.b\td1,(a0)");
    CHECK(dis({0xE140}) == "asl.w\t#8,d0");
    CHECK(dis({0xE3A9}) == "lsl.l\td1,d1");
    CHECK(dis({0xE1D0}) == "asl.w\t(a0)");
    CHECK(dis({0x023C, 0x001F}) == "andi.b\t#$1F,ccr");
    CHECK(dis({0x46FC, 0x2700}) == "move.w\t#$2700,sr");
    CHECK(dis({0x4E68}) == "move.l\tusp,a0");
    CHECK(dis({0x4E56, 0xFFF0}) == "link.w\ta6,#-$10");
    CHECK(dis({0x4E4F}) == "trap\t#15");
    CHECK(dis({0xC189}) == "exg.l\td0,a1");
    CHECK(dis({0x4AFC}) == "illegal");
    CHECK(dis({0xA000}) == "dc.w\t$A000");
    CHECK(dis({0x0000, 0x0112}) == "dc.w\t$0000");  // ori.b with a non-zero immediate high byte
}

TEST_CASE("Flow information and symbols", "[disasm]") {
    const auto bsr = disassemble(words({0x6100, 0x0010}), 0x2000);
    CHECK(bsr.flow == FlowType::Call);
    CHECK(bsr.target == 0x2012u);
    CHECK(bsr.length == 4);
    CHECK(disassemble(words({0x4E75}), 0).flow == FlowType::Return);
    CHECK(disassemble(words({0x4ED0}), 0).flow == FlowType::JumpIndirect);
    CHECK(disassemble(words({0x4E90}), 0).flow == FlowType::CallIndirect);
    CHECK(disassemble(words({0x6700, 0x0004}), 0).flow == FlowType::ConditionalBranch);
    CHECK(disassemble(words({0x4E40}), 0).flow == FlowType::Trap);

    const auto jsr = disassemble(words({0x4EB9, 0x0000, 0x1234}), 0);
    CHECK(jsr.flow == FlowType::Call);
    CHECK(jsr.target == 0x1234u);

    const SymbolResolver symbols = [](std::uint32_t a) -> std::optional<std::string> {
        if (a == 0xFFFFF600) {
            return "Game_Mode";
        }
        if (a == 0x1234) {
            return "Sonic_Jump";
        }
        return std::nullopt;
    };
    CHECK(disassemble(words({0x1038, 0xF600}), 0, symbols).text() == "move.b\t(Game_Mode).w,d0");
    CHECK(disassemble(words({0x4EB9, 0x0000, 0x1234}), 0, symbols).text() == "jsr\t(Sonic_Jump).l");
    const auto ref = disassemble(words({0x1038, 0xF600}), 0);
    REQUIRE(ref.references.size() == 1);
    CHECK(ref.references[0] == 0xFFFFF600u);
}
