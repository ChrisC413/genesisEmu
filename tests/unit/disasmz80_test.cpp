// Z80 disassembler cases (M04-06): syntax, lengths, flow and targets.

#include "analysis/disasmz80/disasmz80.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

namespace dz = gen::analysis::disasmz80;

namespace {
dz::Instruction dis(std::vector<std::uint8_t> bytes, std::uint16_t at = 0x1000) {
    return dz::disassemble(bytes, at);
}
}  // namespace

TEST_CASE("Z80 disassembly uses AS syntax", "[disasm][z80]") {
    CHECK(dis({0xDD, 0x77, 0x05}).text() == "ld\t(ix+$05),a");
    CHECK(dis({0xFD, 0x7E, 0x80}).text() == "ld\ta,(iy-$80)");
    CHECK(dis({0x3A, 0x34, 0x12}).text() == "ld\ta,($1234)");
    CHECK(dis({0xDD, 0x67}).text() == "ld\tixh,a");
    CHECK(dis({0xDD, 0x66, 0x02}).text() == "ld\th,(ix+$02)");  // H stays H next to (IX+d)
    CHECK(dis({0xCB, 0x30}).text() == "sli\tb");
    CHECK(dis({0xDD, 0xCB, 0x03, 0x00}).text() == "rlc\t(ix+$03),b");
    CHECK(dis({0xFD, 0xCB, 0xFF, 0x7E}).text() == "bit\t7,(iy-$01)");
    CHECK(dis({0xED, 0x70}).text() == "in\tf,(c)");
    CHECK(dis({0xED, 0x71}).text() == "out\t(c),0");
    CHECK(dis({0xED, 0xB0}).text() == "ldir");
    CHECK(dis({0x08}).text() == "ex\taf,af'");
}

TEST_CASE("Z80 flow, targets and lengths", "[disasm][z80]") {
    auto jr = dis({0x18, 0xFE});
    CHECK(jr.length == 2);
    CHECK(jr.flow == dz::FlowType::Branch);
    CHECK(jr.target == std::optional<std::uint32_t>(0x1000));
    CHECK(jr.text() == "jr\t$1000");
    CHECK(dis({0x10, 0x00}).flow == dz::FlowType::ConditionalBranch);
    auto call = dis({0xCD, 0x00, 0x20});
    CHECK(call.flow == dz::FlowType::Call);
    CHECK(call.target == std::optional<std::uint32_t>(0x2000));
    CHECK(dis({0xC9}).flow == dz::FlowType::Return);
    CHECK(dis({0xC8}).flow == dz::FlowType::ConditionalBranch);
    CHECK(dis({0xE9}).flow == dz::FlowType::JumpIndirect);
    CHECK(dis({0xFF}).target == std::optional<std::uint32_t>(0x38));
    CHECK(dis({0x76}).flow == dz::FlowType::Stop);
    CHECK(dis({0xDD, 0xCB, 0x01, 0x46}).length == 4);
}

TEST_CASE("Encodings AS cannot reproduce become db", "[disasm][z80]") {
    auto nopPrefix = dis({0xDD, 0x00});
    CHECK(nopPrefix.text() == "db\t$DD");
    CHECK(nopPrefix.length == 1);
    CHECK_FALSE(nopPrefix.valid());
    CHECK(dis({0xED, 0x4C}).text() == "db\t$ED,$4C");                      // NEG mirror
    CHECK(dis({0xED, 0x63, 0x00, 0x10}).text() == "db\t$ED,$63,$00,$10");  // long LD (nn),HL
    CHECK(dis({0xDD, 0xCB, 0x01, 0x40}).length == 4);                      // BIT with a register field
    CHECK(dis({0x21, 0x34}).text() == "db\t$21");                          // truncated operand
}
