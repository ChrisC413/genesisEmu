// [rom] scripted-input regressions (M04-03 / M04-09): every tests/scripts/*.txt names its image by SHA-1, plays
// its input from power on and must reproduce the recorded frame / state hashes at each checkpoint.

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include "common/input_script.hpp"
#include "common/paths.hpp"
#include "core/sched/machine.hpp"
#include "core/state/state_hash.hpp"
#include "rom_scan.hpp"

namespace fs = std::filesystem;
using namespace gen;

TEST_CASE("Scripted input reaches the recorded checkpoints", "[rom][input]") {
    const auto romDir = common::getEnvPath("GENESIS_ROM_DIR");
    if (!romDir || !fs::is_directory(*romDir)) {
        SKIP("GENESIS_ROM_DIR is not set or not a directory");
    }
    const auto roms = test::scanRoms(*romDir);
    std::vector<fs::path> scripts;
    for (const auto& e : fs::directory_iterator(fs::path(GENESIS_SOURCE_DIR) / "tests" / "scripts")) {
        if (e.path().extension() == ".txt") {
            scripts.push_back(e.path());
        }
    }
    std::sort(scripts.begin(), scripts.end());
    REQUIRE_FALSE(scripts.empty());
    int tested = 0;
    for (const auto& path : scripts) {
        INFO(path.filename().string());
        std::ifstream in(path, std::ios::binary);
        const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        std::string error;
        const auto script = common::parseInputScript(text, error);
        REQUIRE(script);
        REQUIRE_FALSE(script->romSha1.empty());
        const auto it = roms.find(script->romSha1);
        if (it == roms.end()) {
            UNSCOPED_INFO("not found: " << script->romSha1);
            continue;
        }
        ++tested;
        Machine m(it->second.image, it->second.regions);
        common::InputPlayer player(*script);
        const std::uint64_t end = script->lastFrame();
        while (m.frame() < end) {
            const std::uint64_t before = m.frame();
            player.beforeFrame(m);
            m.run(RunLimit::frames(1));
            for (const auto& c : script->checkpoints) {
                if (c.frame <= before || c.frame > m.frame() || c.kind == common::CheckKind::AudioHash) {
                    continue;
                }
                INFO("line " << c.line << ": checkpoint " << c.frame << " " << common::toString(c.kind));
                REQUIRE(c.expected);
                const std::uint64_t h =
                    c.kind == common::CheckKind::FrameHash ? m.vdp().frameHash() : m.stateHash();
                CHECK(state::hashToHex(h) == *c.expected);
            }
        }
        CHECK(m.stats().faults == 0);
    }
    if (tested == 0) {
        SKIP("none of the scripted games was found in GENESIS_ROM_DIR");
    }
}
