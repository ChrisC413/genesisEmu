// Scripted input format (M04-03, spec 13).

#include "common/input_script.hpp"

#include <string>

#include <catch2/catch_test_macros.hpp>

#include "core/bus/io.hpp"
#include "core/cart/rom.hpp"
#include "core/sched/machine.hpp"
#include "support/synthetic_rom.hpp"

using namespace gen;

TEST_CASE("Input scripts parse button lines and checkpoints", "[input][script]") {
    const std::string text =
        "# frame  port  buttons\n"
        "0        1     -\n"
        "300      1     RIGHT\n"
        "120      1     START   # out of order is fine\n"
        "420      2     right+c\n"
        "checkpoint 600 frame_hash   # assert the frame hash at frame 600\n"
        "checkpoint 600 state_hash 0123456789ABCDEF\n";
    std::string error;
    const auto s = common::parseInputScript(text, error);
    REQUIRE(s);
    REQUIRE(s->events.size() == 4);
    CHECK(s->events[1].frame == 120);
    CHECK(s->events[1].buttons == bus::kPadStart);
    CHECK(s->events[3].port == 1);
    CHECK(s->events[3].buttons == (bus::kPadRight | bus::kPadC));
    REQUIRE(s->checkpoints.size() == 2);
    CHECK_FALSE(s->checkpoints[0].expected);
    CHECK(s->checkpoints[1].kind == common::CheckKind::StateHash);
    CHECK(*s->checkpoints[1].expected == "0123456789abcdef");
    CHECK(s->lastFrame() == 600);

    const std::string updated = common::updateScriptHashes(text, {{6, "aaaaaaaaaaaaaaaa"}});
    CHECK(updated.find("checkpoint 600 frame_hash aaaaaaaaaaaaaaaa  # assert") != std::string::npos);
    REQUIRE(common::parseInputScript(updated, error));
}

TEST_CASE("Input script errors name the line", "[input][script]") {
    std::string error;
    CHECK_FALSE(common::parseInputScript("0 1 -\n10 3 A\n", error));
    CHECK(error.starts_with("line 2:"));
    CHECK_FALSE(common::parseInputScript("10 1 JUMP\n", error));
    CHECK_FALSE(common::parseInputScript("checkpoint 5 vram_hash\n", error));
}

TEST_CASE("InputPlayer latches buttons at the frame they are scripted for", "[input][script]") {
    std::string error;
    const auto s = common::parseInputScript("0 1 -\n3 1 A+B\n5 2 START\n", error);
    REQUIRE(s);
    Machine m(test::makeSyntheticRom(), cart::kRegionAmericas);
    common::InputPlayer player(*s);
    std::uint16_t latchedFrame4 = 0;
    std::uint16_t latchedFrame3 = 0;
    while (m.frame() < 6) {
        player.beforeFrame(m);
        m.run(RunLimit::frames(1));
        if (m.frame() == 3) {
            latchedFrame3 =
                m.io().state().buttons[0];  // frame 2 has just completed; frame 3 latched at its start
        }
        if (m.frame() == 4) {
            latchedFrame4 = m.io().state().buttons[0];
        }
    }
    CHECK(latchedFrame3 == (bus::kPadA | bus::kPadB));  // latched at the 2 -> 3 boundary
    CHECK(latchedFrame4 == (bus::kPadA | bus::kPadB));
    CHECK(m.io().state().buttons[1] == bus::kPadStart);
}
