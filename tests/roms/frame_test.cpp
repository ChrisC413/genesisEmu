// [rom] golden frame checkpoints (M03-11): each canonical game runs from power-on with no input and its
// frame hash is compared at the checkpoints in tests/golden/m03_frames.json (hashes only). Matching frames
// are cached outside the repository; a mismatch writes actual (and, when cached, expected and diff) PNGs to
// a temporary directory and names them in the failure message.

#include <filesystem>
#include <string>

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include "common/golden_frames.hpp"
#include "common/paths.hpp"
#include "core/sched/machine.hpp"
#include "core/state/state_hash.hpp"
#include "rom_scan.hpp"

namespace fs = std::filesystem;
using namespace gen;

TEST_CASE("Golden frame checkpoints of the SEGA logo, title screens and attract demos", "[rom][video]") {
    const auto romDir = common::getEnvPath("GENESIS_ROM_DIR");
    if (!romDir || !fs::is_directory(*romDir)) {
        SKIP("GENESIS_ROM_DIR is not set or not a directory");
    }
    const auto roms = test::scanRoms(*romDir);
    const auto golden = common::loadFrameGolden(fs::path(GENESIS_SOURCE_DIR) / "tests" / "golden" / "m03_frames.json");
    const fs::path diffDir = fs::temp_directory_path() / "genesisEmu-frame-diffs";
    int tested = 0;
    for (const auto& game : golden) {
        const auto it = roms.find(game.sha1);
        if (it == roms.end()) {
            UNSCOPED_INFO("not found: " << game.name);
            continue;
        }
        ++tested;
        INFO(game.name);
        CHECK(game.checkpoints.size() >= 5);
        Machine m(it->second.image, it->second.regions);
        std::size_t next = 0;
        while (next < game.checkpoints.size()) {
            const std::uint64_t before = m.frame();
            m.run(RunLimit::frames(1));
            // A DMA can carry the run past a frame boundary; a checkpoint is checked when it is crossed.
            while (next < game.checkpoints.size() && game.checkpoints[next].frame <= m.frame()) {
                const auto& cp = game.checkpoints[next++];
                if (cp.frame <= before) {
                    continue;
                }
                const std::string got = state::hashToHex(m.vdp().frameHash());
                INFO("frame " << cp.frame);
                if (got == cp.hash) {
                    common::cacheGoldenFrame(game.sha1, cp, m.vdp());
                } else {
                    const auto stem = game.sha1.substr(0, 8) + "_frame_" + std::to_string(cp.frame);
                    for (const auto& file : common::writeFrameMismatch(diffDir, stem, game.sha1, cp, m.vdp())) {
                        UNSCOPED_INFO("wrote " << common::toUtf8(file));
                    }
                }
                CHECK(got == cp.hash);
            }
        }
        CHECK(m.stats().faults == 0);
    }
    if (tested == 0) {
        SKIP("None of the canonical ROMs (D-023) were found in GENESIS_ROM_DIR");
    }
}
