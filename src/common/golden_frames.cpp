#include "common/golden_frames.hpp"

#include <algorithm>
#include <fstream>
#include <stdexcept>
#include <system_error>

#include <nlohmann/json.hpp>

#include "common/paths.hpp"
#include "common/png.hpp"
#include "core/vdp/vdp.hpp"

namespace gen::common {

namespace fs = std::filesystem;

namespace {

fs::path cacheFile(const std::string& sha1, const FrameCheckpoint& cp) {
    return goldenFrameCache() / sha1 / ("frame_" + std::to_string(cp.frame) + "_" + cp.hash + ".rgba");
}

}  // namespace

std::vector<GoldenGame> loadFrameGolden(const fs::path& path) {
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("cannot open " + toUtf8(path));
    }
    const nlohmann::json j = nlohmann::json::parse(in);
    std::vector<GoldenGame> games;
    for (const auto& g : j.at("games")) {
        GoldenGame game;
        game.name = g.at("name").get<std::string>();
        game.sha1 = g.at("sha1").get<std::string>();
        for (const auto& c : g.at("checkpoints")) {
            game.checkpoints.push_back({c.at("frame").get<std::uint64_t>(), c.at("frameHash").get<std::string>()});
        }
        games.push_back(std::move(game));
    }
    return games;
}

fs::path goldenFrameCache() {
    if (const auto dir = getEnvPath("GENESIS_GOLDEN_CACHE")) {
        return *dir;
    }
    std::error_code ec;
    const fs::path tmp = fs::temp_directory_path(ec);
    return (ec ? fs::path(".") : tmp) / "genesisEmu-golden-frames";
}

void cacheGoldenFrame(const std::string& sha1, const FrameCheckpoint& cp, const vdp::Vdp& vdp) {
    const fs::path file = cacheFile(sha1, cp);
    std::error_code ec;
    if (fs::exists(file, ec)) {
        return;
    }
    fs::create_directories(file.parent_path(), ec);
    std::ofstream out(file, std::ios::binary);
    const std::int32_t dims[2] = {vdp.frameWidth(), vdp.frameHeight()};
    out.write(reinterpret_cast<const char*>(dims), sizeof dims);
    for (int y = 0; y < vdp.frameHeight(); ++y) {
        out.write(reinterpret_cast<const char*>(vdp.framebuffer() + static_cast<std::size_t>(y) * vdp::kFbWidth),
                  static_cast<std::streamsize>(vdp.frameWidth()) * 4);
    }
}

std::vector<fs::path> writeFrameMismatch(const fs::path& dir, const std::string& stem, const std::string& sha1,
                                         const FrameCheckpoint& cp, const vdp::Vdp& vdp) {
    std::vector<fs::path> written;
    std::error_code ec;
    fs::create_directories(dir, ec);
    const int w = vdp.frameWidth();
    const int h = vdp.frameHeight();
    const fs::path actual = dir / (stem + "_actual.png");
    if (writePng(actual, vdp.framebuffer(), w, h, vdp::kFbWidth)) {
        written.push_back(actual);
    }
    std::ifstream in(cacheFile(sha1, cp), std::ios::binary);
    std::int32_t dims[2] = {0, 0};
    if (!in.read(reinterpret_cast<char*>(dims), sizeof dims) || dims[0] <= 0 || dims[1] <= 0 ||
        dims[0] > vdp::kFbWidth || dims[1] > vdp::kFbHeight) {
        return written;  // no cached expected frame
    }
    std::vector<std::uint32_t> expected(static_cast<std::size_t>(dims[0]) * static_cast<std::size_t>(dims[1]));
    if (!in.read(reinterpret_cast<char*>(expected.data()), static_cast<std::streamsize>(expected.size() * 4))) {
        return written;
    }
    const fs::path exp = dir / (stem + "_expected.png");
    if (writePng(exp, expected.data(), dims[0], dims[1], dims[0])) {
        written.push_back(exp);
    }
    // Diff over the common area: red where pixels differ, the actual frame dimmed elsewhere.
    const int dw = std::max(w, dims[0]);
    const int dh = std::max(h, dims[1]);
    std::vector<std::uint32_t> diff(static_cast<std::size_t>(dw) * static_cast<std::size_t>(dh), 0xFF000000u);
    for (int y = 0; y < dh; ++y) {
        for (int x = 0; x < dw; ++x) {
            const bool inA = x < w && y < h;
            const bool inE = x < dims[0] && y < dims[1];
            const std::uint32_t a = inA ? vdp.framebuffer()[static_cast<std::size_t>(y) * vdp::kFbWidth + x] : 0;
            const std::uint32_t e = inE ? expected[static_cast<std::size_t>(y) * dims[0] + x] : 0;
            std::uint32_t& d = diff[static_cast<std::size_t>(y) * dw + x];
            if (inA != inE || a != e) {
                d = 0xFF0000FFu;  // red (R in the low byte)
            } else {
                d = ((a >> 2) & 0x003F3F3Fu) | 0xFF000000u;
            }
        }
    }
    const fs::path df = dir / (stem + "_diff.png");
    if (writePng(df, diff.data(), dw, dh, dw)) {
        written.push_back(df);
    }
    return written;
}

}  // namespace gen::common
