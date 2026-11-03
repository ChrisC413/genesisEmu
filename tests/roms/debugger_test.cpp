// [rom] debugger acceptance tests (M06-02/03/04/07/10/11) in Sonic 1 REV01, driven by the scripted input of
// tests/scripts/sonic1_ghz1.txt (title screen -> Green Hill Zone act 1, running and jumping). Routines are
// located by byte patterns of the original code, so no addresses of the commercial ROM are hard-coded.

#include "debug/debugger.hpp"

#include <chrono>
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
#include "debug/listing.hpp"
#include "debug/memory.hpp"
#include "rom_scan.hpp"

namespace fs = std::filesystem;
using namespace gen;
using namespace gen::debug;

namespace {

constexpr const char* kSonic1Sha1 = "69e102855d4389c3fd1a8f3dc7d193f8eee5fe5b";

struct Sonic1 {
    test::GameImage game;
    common::InputScript script;
};

std::optional<Sonic1> loadSonic1() {
    const auto romDir = common::getEnvPath("GENESIS_ROM_DIR");
    if (!romDir || !fs::is_directory(*romDir)) {
        return std::nullopt;
    }
    auto roms = test::scanRoms(*romDir);
    const auto it = roms.find(kSonic1Sha1);
    if (it == roms.end()) {
        return std::nullopt;
    }
    std::ifstream in(fs::path(GENESIS_SOURCE_DIR) / "tests" / "scripts" / "sonic1_ghz1.txt",
                     std::ios::binary);
    const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    std::string error;
    auto script = common::parseInputScript(text, error);
    if (!script) {
        return std::nullopt;
    }
    return Sonic1{std::move(it->second), std::move(*script)};
}

std::uint32_t findOnce(const Machine& m, const char* pattern) {
    std::string error;
    const auto p = parsePattern(pattern, error);
    REQUIRE(p);
    const auto hits = findAll(m, Region::Rom, *p);
    INFO(pattern);
    REQUIRE(hits.size() == 1);
    return static_cast<std::uint32_t>(hits[0]);
}

// Plays the script through the debugger until it stops or `until` is reached.
RunResult play(Debugger& d, common::InputPlayer& player, std::uint64_t until) {
    Machine& m = d.machine();
    while (m.frame() < until) {
        player.beforeFrame(m);
        const RunResult r = d.run(RunLimit::frames(1));
        m.sound().clearSamples();
        if (r.reason != StopReason::LimitReached) {
            return r;
        }
    }
    return {};
}

}  // namespace

TEST_CASE("Sonic 1: breakpoint on the jump routine stops when Sonic jumps", "[rom][debug]") {
    const auto s1 = loadSonic1();
    if (!s1) {
        SKIP("Sonic 1 REV01 not in GENESIS_ROM_DIR");
    }
    Machine m(s1->game.image, s1->game.regions);
    Debugger d(m);
    common::InputPlayer player(s1->script);
    // Sonic_Jump: move.b (v_jpadpress2).w,d0 ; andi.b #btnABC,d0 ; beq.w (return)
    const std::uint32_t jump = findOnce(m, "1038 F603 0200 0070 6700");
    const std::uint32_t jumping = jump + 12;  // the code after the beq.w runs only when a jump starts
    std::string error;
    Breakpoint bp;
    bp.kind = BpKind::Exec;
    bp.start = bp.end = jumping;
    const int id = d.add(bp, error);
    REQUIRE(id > 0);
    const RunResult r = play(d, player, 1300);
    REQUIRE(r.reason == StopReason::Breakpoint);
    CHECK(m.cpu().state().pc == jumping);
    // The script presses C at frame 1000 (RIGHT+C), the first jump input in a level.
    CHECK(m.frame() >= 1000);
    CHECK(m.frame() <= 1003);
    CHECK(d.lastStop().breakpoint == id);
    // A condition on a register: d0 holds the pressed A/B/C bits there (C = $20).
    Breakpoint c = *d.find(id);
    c.condition = "d0.b == $20";
    REQUIRE(d.update(c, error));
    CHECK(play(d, player, 1300).reason == StopReason::Breakpoint);
    CHECK((m.cpu().state().d[0] & 0xFF) == 0x20);

    // Call stack inside the object routine: level main loop -> ExecuteObjects (jsr (a1)) -> Sonic.
    d.setCallStackTracking(true);
    Breakpoint top;
    top.kind = BpKind::Exec;
    top.start = top.end = jump;
    d.clearBreakpoints();
    d.add(top, error);
    // Tracking began mid-frame; the next frame's main loop builds the exact stack.
    play(d, player, m.frame() + 1);
    REQUIRE(play(d, player, 1300).reason == StopReason::Breakpoint);
    const auto stack = d.callStack();
    REQUIRE(stack.size() >= 3);
    bool viaExecuteObjects = false;
    for (const CallFrame& f : stack) {
        if (peekValue(m, CpuId::M68k, f.callPc, 2) == 0x4E91) {  // jsr (a1)
            viaExecuteObjects = true;
        }
        CHECK(f.kind == CallFrame::Kind::Call);
    }
    CHECK(viaExecuteObjects);
    // The heuristic walk agrees on the outermost frames.
    const auto walked = d.walkStack();
    REQUIRE(walked.size() >= 3);
    CHECK(walked.back().returnPc == stack.back().returnPc);
    // Disassembly listing around the PC lines up with the instruction starts that executed.
    KnownStarts known;
    known.add(CpuId::M68k, m.cpu().state().pc);
    const auto lines = buildListing(m, CpuId::M68k, m.cpu().state().pc, 8, 16, known);
    REQUIRE(lines.size() == 16);
    CHECK(lines[8].addr == m.cpu().state().pc);
    CHECK(lines[8].text.starts_with("move.b"));
}

TEST_CASE("Sonic 1: write watch on the ring counter stops in the ring-collect code", "[rom][debug]") {
    const auto s1 = loadSonic1();
    if (!s1) {
        SKIP("Sonic 1 REV01 not in GENESIS_ROM_DIR");
    }
    Machine m(s1->game.image, s1->game.regions);
    Debugger d(m);
    common::InputPlayer player(s1->script);
    play(d, player, 900);  // in the level, before Sonic starts running
    std::string error;
    Breakpoint wp;
    wp.kind = BpKind::Write;
    wp.start = 0xFFFE20;  // v_rings (word)
    wp.end = 0xFFFE21;
    wp.condition = "value != 0";
    REQUIRE(d.add(wp, error) > 0);
    const RunResult r = play(d, player, 1300);
    REQUIRE(r.reason == StopReason::Watchpoint);
    // The accessing instruction is addq.w #1,(v_rings).w in CollectRing.
    CHECK(peekValue(m, CpuId::M68k, d.lastStop().pc, 4) == 0x5278FE20);
    CHECK(d.lastStop().value == 1);
    CHECK(m.bus().state().ram[0xFE21] == 1);
}

TEST_CASE("Sonic 1: DMA-to-CRAM breakpoint fires on palette loads; sprites decode", "[rom][debug]") {
    const auto s1 = loadSonic1();
    if (!s1) {
        SKIP("Sonic 1 REV01 not in GENESIS_ROM_DIR");
    }
    Machine m(s1->game.image, s1->game.regions);
    Debugger d(m);
    common::InputPlayer player(s1->script);
    play(d, player, 1000);
    std::string error;
    Breakpoint dma;
    dma.kind = BpKind::Dma;
    dma.dmaTarget = static_cast<int>(vdp::DmaTarget::Cram);
    dma.start = 0;
    dma.end = 0x7F;
    REQUIRE(d.add(dma, error) > 0);
    const std::uint64_t frame = m.frame();
    REQUIRE(play(d, player, 1100).reason == StopReason::Watchpoint);
    CHECK(m.frame() <= frame + 1);  // every frame's V-int uploads the palette
    CHECK(d.lastStop().address == 0);
    CHECK(d.lastStop().value >= 0xFF0000);  // from the palette buffer in RAM
    d.clearBreakpoints();
    play(d, player, 1100);
    // Sprites on screen in GHZ: Sonic plus HUD / rings / badniks, all with sane sizes.
    const auto sprites = m.vdp().decodeSprites();
    CHECK(sprites.size() >= 5);
    for (const vdp::SpriteInfo& s : sprites) {
        CHECK(s.widthCells >= 1);
        CHECK(s.widthCells <= 4);
    }
}

TEST_CASE("Sonic 1: step over a jsr and 100 breakpoints within the performance budget",
          "[rom][debug][perf]") {
    const auto s1 = loadSonic1();
    if (!s1) {
        SKIP("Sonic 1 REV01 not in GENESIS_ROM_DIR");
    }
    Machine m(s1->game.image, s1->game.regions);
    Debugger d(m);
    common::InputPlayer player(s1->script);
    play(d, player, 1000);
    // Step until a jsr is next, then step over it (interrupts may be taken inside).
    for (int i = 0; i < 100000 && (m.cpu().state().ir & 0xFFC0) != 0x4E80; ++i) {
        d.stepInto(CpuId::M68k);
    }
    REQUIRE((m.cpu().state().ir & 0xFFC0) == 0x4E80);
    const std::uint32_t pc = m.cpu().state().pc;
    const std::uint32_t sp = m.cpu().state().a[7];
    int length = 0;
    const auto line = disassembleAt(m, CpuId::M68k, pc);
    length = line.length;
    d.stepOver(CpuId::M68k);
    CHECK(m.cpu().state().pc == pc + static_cast<std::uint32_t>(length));
    CHECK(m.cpu().state().a[7] == sp);

    // 100 exec breakpoints that never hit, plus the frame-time comparison (spec 01: < 50% of a core at
    // 60 fps, i.e. well under 8 ms per frame).
    const auto timeFrames = [&](int n) {
        const auto t0 = std::chrono::steady_clock::now();
        for (int i = 0; i < n; ++i) {
            player.beforeFrame(m);
            d.run(RunLimit::frames(1));
            m.sound().clearSamples();
        }
        return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count() / n;
    };
    const double plain = timeFrames(120);
    std::string error;
    for (std::uint32_t i = 0; i < 100; ++i) {
        Breakpoint bp;
        bp.kind = BpKind::Exec;
        bp.start = bp.end = 0x3F0000 + i * 2;  // beyond the 512 KB ROM: never executed
        REQUIRE(d.add(bp, error) > 0);
    }
    const double with = timeFrames(120);
    WARN("frame time: " << plain << " ms without breakpoints, " << with << " ms with 100 exec breakpoints");
    CHECK(with < 8.0);
}
