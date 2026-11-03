// [rom] save-state determinism (M06-00a, spec 08): for every scripted game in tests/scripts/ whose ROM is in
// GENESIS_ROM_DIR, play the script to its last frame (in a level), save, run 600 more frames, then load the
// state into a freshly powered-on machine, run the same 600 frames and require identical state and frame
// hashes.

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

namespace {

constexpr std::uint64_t kFramesAfterSave = 600;

struct Hashes {
    std::uint64_t state = 0;
    std::uint64_t frame = 0;
};

Hashes runScript(Machine& m, const common::InputScript& script, std::uint64_t until) {
    common::InputPlayer player(script);
    while (m.frame() < until) {
        player.beforeFrame(m);
        m.run(RunLimit::frames(1));
        m.sound().clearSamples();
    }
    return {m.stateHash(), m.vdp().frameHash()};
}

}  // namespace

TEST_CASE("Save -> run 600 frames == load -> run 600 frames in the scripted games", "[rom][state]") {
    const auto romDir = common::getEnvPath("GENESIS_ROM_DIR");
    if (!romDir || !fs::is_directory(*romDir)) {
        SKIP("GENESIS_ROM_DIR is not set or not a directory");
    }
    const auto roms = test::scanRoms(*romDir);
    int tested = 0;
    for (const auto& e : fs::directory_iterator(fs::path(GENESIS_SOURCE_DIR) / "tests" / "scripts")) {
        if (e.path().extension() != ".txt") {
            continue;
        }
        INFO(e.path().filename().string());
        std::ifstream in(e.path(), std::ios::binary);
        const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        std::string error;
        const auto script = common::parseInputScript(text, error);
        REQUIRE(script);
        const auto it = roms.find(script->romSha1);
        if (it == roms.end()) {
            continue;
        }
        ++tested;
        const std::uint64_t saveAt = script->lastFrame();
        Machine m(it->second.image, it->second.regions);
        runScript(m, *script, saveAt);
        const std::vector<std::uint8_t> saved = m.saveState();
        const Hashes first = runScript(m, *script, saveAt + kFramesAfterSave);

        Machine fresh(it->second.image, it->second.regions);
        REQUIRE(fresh.loadState(saved, error));
        CHECK(fresh.frame() == saveAt);
        const Hashes second = runScript(fresh, *script, saveAt + kFramesAfterSave);
        CHECK(state::hashToHex(second.state) == state::hashToHex(first.state));
        CHECK(state::hashToHex(second.frame) == state::hashToHex(first.frame));

        // Loading into the machine that ran on (not a fresh one) gives the same result.
        REQUIRE(m.loadState(saved, error));
        const Hashes third = runScript(m, *script, saveAt + kFramesAfterSave);
        CHECK(third.state == first.state);
    }
    if (tested == 0) {
        SKIP("none of the scripted games was found in GENESIS_ROM_DIR");
    }
}
