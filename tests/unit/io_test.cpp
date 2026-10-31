// Controller port protocol (M04-01): the read sequences games use for 3- and 6-button pads.

#include <catch2/catch_test_macros.hpp>

#include "core/bus/io.hpp"

using namespace gen::bus;

namespace {

constexpr std::uint32_t kData1 = 1;  // register index of $A10003
constexpr std::uint32_t kCtrl1 = 4;  // $A10009

Io makeIo(PadType type) {
    Io io;
    io.setPadType(0, type);
    io.reset(Region::Americas, false, false);
    io.write(kData1, 0x40, 0);  // TH high, then TH = output, as games set it up
    io.write(kCtrl1, 0x40, 0);
    return io;
}

// Reads port 1 with TH = `th` at master clock `t`; returns bits 0-5 (bit 6 echoes TH).
std::uint8_t readTh(Io& io, bool th, std::uint64_t t) {
    io.write(kData1, th ? 0x40 : 0x00, t);
    return static_cast<std::uint8_t>(io.read(kData1, t) & 0x3F);
}

}  // namespace

TEST_CASE("3-button pad: Sonic's two-phase read", "[io][pad]") {
    Io io = makeIo(PadType::ThreeButton);
    io.state().buttons[0] = kPadRight | kPadC | kPadStart;
    // Sonic 1/2 (ReadJoypads): TH=0 read -> Start/A, TH=1 read -> C/B/directions. Active low.
    CHECK(readTh(io, false, 100) == 0x13);  // S pressed (bit 5 low), A released, bits 3-2 low, D U released
    CHECK(readTh(io, true, 200) == 0x17);   // C pressed (bit 5 low), B released, Right pressed (bit 3 low)
    // A 3-button pad never shows the 6-button identification, however often TH toggles.
    for (int i = 0; i < 4; ++i) {
        CHECK(readTh(io, false, 300 + i * 20) == 0x13);
        CHECK(readTh(io, true, 310 + i * 20) == 0x17);
    }
}

TEST_CASE("6-button pad: identification and extra buttons, then timeout reset", "[io][pad]") {
    Io io = makeIo(PadType::SixButton);
    io.state().buttons[0] = kPadA | kPadUp | kPadX | kPadMode;
    std::uint64_t t = 1000;
    const auto step = [&] { return t += 100; };
    // Phases 1-2: normal 3-button data.
    CHECK(readTh(io, true, step()) == 0x3E);   // 1 C B R L D U: Up pressed
    CHECK(readTh(io, false, step()) == 0x22);  // 0 S A 0 0 D U: A and Up pressed
    CHECK(readTh(io, true, step()) == 0x3E);
    CHECK(readTh(io, false, step()) == 0x22);
    CHECK(readTh(io, true, step()) == 0x3E);
    // Phase 3 low: bits 0-3 all low identify a 6-button pad.
    CHECK(readTh(io, false, step()) == 0x20);
    // Phase 4 high: C B Mode X Y Z.
    CHECK(readTh(io, true, step()) == 0x33);  // Mode (bit 3) and X (bit 2) pressed
    // Phase 4 low: 0 S A 1 1 1 1.
    CHECK(readTh(io, false, step()) == 0x2F);
    CHECK(readTh(io, true, step()) == 0x3E);  // back to normal data
    // After ~1.5 ms without TH changes the counter resets: the next cycle starts over.
    t += kSixButtonTimeoutMclk + 1;
    CHECK(readTh(io, true, step()) == 0x3E);
    CHECK(readTh(io, false, step()) == 0x22);  // phase 1 again, not the identification pattern
}

TEST_CASE("Ports without a pad and port 3 read as pulled-up inputs", "[io][pad]") {
    Io io = makeIo(PadType::None);
    io.state().buttons[0] = kPadStart;
    CHECK(readTh(io, false, 10) == 0x3F);
    CHECK((io.read(3, 10) & 0x7F) == 0x7F);  // EXT port
}
