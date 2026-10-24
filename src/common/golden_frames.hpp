#pragma once
// Golden frame checkpoints (M03-11, spec 13): frame-hash checkpoints per game, checked by `gen_cli run
// --check` and the [rom] frame tests. Golden files hold hashes only; frames that match are cached locally
// (outside the repository) so a later mismatch can write expected / actual / diff PNGs.

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace gen::vdp {
class Vdp;
}

namespace gen::common {

struct FrameCheckpoint {
    std::uint64_t frame = 0;  // checked after this many frames have run from power-on
    std::string hash;         // Vdp::frameHash() as 16 hex digits
};

struct GoldenGame {
    std::string name;
    std::string sha1;  // of the ROM image (lock-on: the combined image)
    std::vector<FrameCheckpoint> checkpoints;
};

// Throws std::runtime_error when the file cannot be read or parsed.
std::vector<GoldenGame> loadFrameGolden(const std::filesystem::path& path);

// Where matching frames are cached: $GENESIS_GOLDEN_CACHE, else <temp>/genesisEmu-golden-frames.
std::filesystem::path goldenFrameCache();

// Stores a frame that matched its checkpoint (raw RGBA) for later diffs.
void cacheGoldenFrame(const std::string& sha1, const FrameCheckpoint& cp, const vdp::Vdp& vdp);

// Writes <dir>/<stem>_actual.png and, when the expected frame is in the cache, <stem>_expected.png and
// <stem>_diff.png (differing pixels in red over the dimmed actual frame). Returns the files written.
std::vector<std::filesystem::path> writeFrameMismatch(const std::filesystem::path& dir, const std::string& stem,
                                                      const std::string& sha1, const FrameCheckpoint& cp,
                                                      const vdp::Vdp& vdp);

}  // namespace gen::common
