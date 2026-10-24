// [rom] boot smoke tests (M02-07): each target game runs 600 frames headless with no fault exceptions,
// ends up in its V-int wait loop, and two runs give the same state hash. ROMs are found in GENESIS_ROM_DIR
// by SHA-1. tests/golden/m02_boot.json stores only numbers per game (SHA-1, wait-loop addresses, state
// hash), never ROM bytes or code.

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include "common/paths.hpp"
#include "core/cart/rom.hpp"
#include "core/sched/machine.hpp"
#include "core/state/state_hash.hpp"

namespace fs = std::filesystem;
using namespace gen;

namespace {

constexpr std::uint64_t kBootFrames = 600;
constexpr std::uint64_t kObservedFrames = 60;  // the wait-loop check looks at the last 60 frames
constexpr std::uint32_t kMaxLoopBytes = 16;

struct GameImage {
    std::vector<std::uint8_t> image;
    std::uint8_t regions = 0;
};

// Every ROM image in GENESIS_ROM_DIR by SHA-1, plus the S&K lock-on combinations.
std::map<std::string, GameImage> scanRoms(const fs::path& dir) {
    std::map<std::string, GameImage> out;
    std::vector<cart::Rom> sk;
    std::vector<cart::Rom> s3;
    for (const auto& entry : fs::recursive_directory_iterator(dir)) {
        if (!entry.is_regular_file() || entry.file_size() > cart::kMaxRomSize + cart::kSmdHeaderSize) {
            continue;
        }
        auto result = cart::loadRomFile(entry.path());
        if (!result || result.rom->known == nullptr) {
            continue;
        }
        cart::Rom& rom = *result.rom;
        if (rom.known->game == cart::Game::SonicAndKnuckles) {
            sk.push_back(rom);
        } else if (rom.known->game == cart::Game::Sonic3) {
            s3.push_back(rom);
        }
        out.emplace(rom.sha1Hex, GameImage{rom.data, rom.header.regions});
    }
    for (const auto& a : sk) {
        for (const auto& b : s3) {
            if (const auto* combined = cart::identifyLockOn(a.data, b.data)) {
                out.emplace(std::string(combined->sha1),
                            GameImage{cart::lockOnImage(a.data, b.data), b.header.regions});
            }
        }
    }
    return out;
}

nlohmann::json loadGolden() {
    std::ifstream in(fs::path(GENESIS_SOURCE_DIR) / "tests" / "golden" / "m02_boot.json");
    REQUIRE(in.good());
    return nlohmann::json::parse(in);
}

// Observes the main program's reads of work RAM inside the candidate loop and the V-int handler's writes.
struct LoopProbe {
    const Machine* machine = nullptr;
    std::uint32_t lo = 0;
    std::uint32_t hi = 0;
    std::set<std::uint32_t> loopReads;
    std::set<std::uint32_t> vintWrites;

    static void onAccess(void* ctx, CpuId, AccessKind kind, std::uint32_t addr, std::uint8_t size,
                         std::uint32_t, std::uint32_t pc) {
        auto& p = *static_cast<LoopProbe*>(ctx);
        if (addr < 0xE00000) {
            return;
        }
        const std::uint32_t ram = 0xFF0000 | (addr & 0xFFFF);
        const int mask = (p.machine->cpu().state().sr >> 8) & 7;
        for (std::uint32_t i = 0; i < size; ++i) {
            if (kind == AccessKind::Read && pc >= p.lo && pc <= p.hi && mask < 6) {
                p.loopReads.insert(ram + i);
            } else if (kind == AccessKind::Write && mask == 6) {
                p.vintWrites.insert(ram + i);
            }
        }
    }
};

struct BootReport {
    std::uint64_t faults = 0;
    bool halted = false;
    std::uint32_t loopLo = 0;
    std::uint32_t loopHi = 0;
    double loopFraction = 0;     // observed frames whose V-int interrupted the loop window
    double oneVintFraction = 0;  // observed frames with exactly one V-int
    bool pollsVintFlag = false;
    std::string hash;
    double msPerFrame = 0;
};

BootReport boot(const GameImage& game, std::uint64_t frames, bool probe) {
    Machine m(game.image, game.regions);
    BootReport r;
    const auto t0 = std::chrono::steady_clock::now();
    m.run(RunLimit::frames(frames - kObservedFrames));
    std::vector<std::uint32_t> vintPcs;
    std::uint64_t oneVint = 0;
    for (std::uint64_t f = 0; f < kObservedFrames; ++f) {
        m.run(RunLimit::frames(1));
        oneVint += m.stats().vintsLastFrame == 1 ? 1 : 0;
        if (m.stats().vintsLastFrame != 0) {
            vintPcs.push_back(m.stats().lastVintPc);
        }
    }
    r.msPerFrame = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count() /
                   static_cast<double>(frames);
    r.faults = m.stats().faults;
    r.halted = m.cpu().state().halted;
    r.hash = state::hashToHex(m.stateHash());
    r.oneVintFraction = static_cast<double>(oneVint) / kObservedFrames;
    if (vintPcs.empty()) {
        return r;
    }
    // Generic wait-loop heuristic: the V-int keeps interrupting the same few bytes of code.
    std::map<std::uint32_t, int> counts;
    for (const std::uint32_t pc : vintPcs) {
        ++counts[pc];
    }
    const auto mode = std::max_element(counts.begin(), counts.end(), [](const auto& a, const auto& b) {
                          return a.second < b.second;
                      })->first;
    r.loopLo = mode;
    r.loopHi = mode;
    int inside = 0;
    for (const std::uint32_t pc : vintPcs) {
        if (pc + kMaxLoopBytes > mode && pc < mode + kMaxLoopBytes) {
            ++inside;
            r.loopLo = std::min(r.loopLo, pc);
            r.loopHi = std::max(r.loopHi, pc);
        }
    }
    r.loopFraction = static_cast<double>(inside) / kObservedFrames;
    if (probe) {
        // The loop must poll a RAM location the V-int handler writes (the "V-int happened" flag).
        LoopProbe p;
        p.machine = &m;
        p.lo = r.loopLo;
        p.hi = r.loopHi + 8;
        Hooks hooks;
        hooks.ctx = &p;
        hooks.onAccess = &LoopProbe::onAccess;
        m.setHooks(hooks);
        m.run(RunLimit::frames(5));
        for (const std::uint32_t a : p.loopReads) {
            r.pollsVintFlag = r.pollsVintFlag || p.vintWrites.contains(a);
        }
    }
    return r;
}

}  // namespace

TEST_CASE("Target games boot to their V-int wait loop in 600 frames, deterministically", "[rom][boot]") {
    const auto romDir = common::getEnvPath("GENESIS_ROM_DIR");
    if (!romDir || !fs::is_directory(*romDir)) {
        SKIP("GENESIS_ROM_DIR is not set or not a directory");
    }
    const auto roms = scanRoms(*romDir);
    const nlohmann::json golden = loadGolden();
    int tested = 0;
    for (const auto& g : golden.at("games")) {
        const std::string name = g.at("name");
        const auto it = roms.find(g.at("sha1").get<std::string>());
        if (it == roms.end()) {
            UNSCOPED_INFO("not found: " << name);
            continue;
        }
        ++tested;
        INFO(name);
        const BootReport a = boot(it->second, kBootFrames, true);
        WARN(name << ": wait loop $" << std::hex << a.loopLo << "-$" << a.loopHi << std::dec << " ("
                  << a.loopFraction * 100 << "% of V-ints), one V-int in " << a.oneVintFraction * 100
                  << "% of frames, polls V-int flag " << a.pollsVintFlag << ", hash " << a.hash << ", "
                  << a.msPerFrame << " ms/frame");
        CHECK(a.faults == 0);
        CHECK_FALSE(a.halted);
        CHECK(a.oneVintFraction >= 0.9);
        CHECK(a.loopFraction >= 0.9);
        CHECK(a.loopHi - a.loopLo < kMaxLoopBytes);
        CHECK(a.pollsVintFlag);
        const auto& loop = g.at("waitLoop");
        CHECK(a.loopLo >= loop.at(0).get<std::uint32_t>());
        CHECK(a.loopHi <= loop.at(1).get<std::uint32_t>());

        const BootReport b = boot(it->second, kBootFrames, false);
        CHECK(a.hash == b.hash);
        CHECK(a.hash == g.at("stateHash").get<std::string>());
    }
    if (tested == 0) {
        SKIP("None of the canonical ROMs (D-023) were found in GENESIS_ROM_DIR");
    }
}

TEST_CASE("Sonic 1 boots with the Japanese and European version registers too", "[rom][boot]") {
    const auto romDir = common::getEnvPath("GENESIS_ROM_DIR");
    if (!romDir || !fs::is_directory(*romDir)) {
        SKIP("GENESIS_ROM_DIR is not set or not a directory");
    }
    const auto roms = scanRoms(*romDir);
    const auto it = roms.find("69e102855d4389c3fd1a8f3dc7d193f8eee5fe5b");
    if (it == roms.end()) {
        SKIP("Sonic 1 REV01 not found");
    }
    for (const auto region : {bus::Region::Japan, bus::Region::Americas, bus::Region::Europe}) {
        INFO(bus::toString(region));
        MachineConfig config;
        config.region = region;
        Machine m(it->second.image, it->second.regions, config);
        m.run(RunLimit::frames(kBootFrames));
        CHECK(m.stats().faults == 0);
        CHECK(m.stats().vintsLastFrame >= 1);
        // The DAC driver was copied into Z80 RAM and art was loaded into VRAM.
        const auto& zram = m.z80().state().ram;
        CHECK(std::count_if(zram.begin(), zram.end(), [](std::uint8_t v) { return v != 0; }) > 256);
        const auto& vram = m.vdp().state().vram;
        CHECK(std::count_if(vram.begin(), vram.end(), [](std::uint8_t v) { return v != 0; }) > 4096);
    }
}
