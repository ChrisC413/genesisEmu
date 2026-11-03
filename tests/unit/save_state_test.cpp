// Save states (M06-00a): archive round trips, the chunked container, chunk versioning, wrong-ROM refusal and
// save -> run == load -> run determinism on the synthetic boot ROM.

#include "core/state/save_state.hpp"

#include <cstdint>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "core/cart/rom.hpp"
#include "core/sched/machine.hpp"
#include "core/state/archive.hpp"
#include "support/synthetic_rom.hpp"

using namespace gen;
using Catch::Matchers::ContainsSubstring;

namespace {

Machine bootMachine() {
    return Machine(test::makeBootTestRom(), cart::kRegionAmericas);
}

struct Snapshot {
    std::uint64_t state = 0;
    std::uint64_t frame = 0;
};

Snapshot runFrames(Machine& m, std::uint64_t n) {
    m.run(RunLimit::frames(n));
    m.sound().clearSamples();
    return {m.stateHash(), m.vdp().frameHash()};
}

// Rewrites one chunk of an encoded state.
std::vector<std::uint8_t> edit(const std::vector<std::uint8_t>& bytes, void (*fn)(state::StateFile&)) {
    std::string error;
    auto file = state::decodeStateFile(bytes, error);
    REQUIRE(file);
    fn(*file);
    return state::encodeStateFile(*file);
}

}  // namespace

TEST_CASE("Archive saves and loads the same fields", "[state]") {
    std::vector<std::uint8_t> out;
    {
        state::Archive ar(out, 3);
        std::uint8_t a = 0x12;
        std::uint16_t b = 0x3456;
        std::uint32_t c = 0x789ABCDE;
        std::uint64_t d = 0x0123456789ABCDEFull;
        int e = -5;
        bool f = true;
        float g = 1.5f;
        std::vector<std::uint8_t> h{1, 2, 3};
        std::string s = "sram";
        ar.u8(a);
        ar.u16(b);
        ar.u32(c);
        ar.u64(d);
        ar.i32(e);
        ar.boolean(f);
        ar.f32(g);
        ar.blob(h);
        ar.string(s);
        CHECK(ar.ok());
        CHECK_FALSE(ar.loading());
    }
    CHECK(out.size() == 1 + 2 + 4 + 8 + 4 + 1 + 4 + (4 + 3) + (4 + 4));
    CHECK(out[1] == 0x56);  // little-endian
    CHECK(out[2] == 0x34);

    state::Archive in(std::span<const std::uint8_t>(out), 3);
    CHECK(in.loading());
    CHECK(in.version() == 3);
    std::uint8_t a = 0;
    std::uint16_t b = 0;
    std::uint32_t c = 0;
    std::uint64_t d = 0;
    int e = 0;
    bool f = false;
    float g = 0;
    std::vector<std::uint8_t> h;
    std::string s;
    in.u8(a);
    in.u16(b);
    in.u32(c);
    in.u64(d);
    in.i32(e);
    in.boolean(f);
    in.f32(g);
    in.blob(h);
    in.string(s);
    CHECK(in.ok());
    CHECK(in.atEnd());
    CHECK(a == 0x12);
    CHECK(b == 0x3456);
    CHECK(c == 0x789ABCDE);
    CHECK(d == 0x0123456789ABCDEFull);
    CHECK(e == -5);
    CHECK(f);
    CHECK(g == 1.5f);
    CHECK(h == std::vector<std::uint8_t>{1, 2, 3});
    CHECK(s == "sram");

    // Reading past the end fails and leaves the field alone.
    std::uint32_t extra = 7;
    in.u32(extra);
    CHECK_FALSE(in.ok());
    CHECK(extra == 7);
}

TEST_CASE("A fixed-size blob refuses another size", "[state]") {
    std::vector<std::uint8_t> out;
    state::Archive ar(out, 1);
    std::vector<std::uint8_t> eight(8, 0xAA);
    ar.fixedBlob(eight);
    std::vector<std::uint8_t> four(4, 0);
    state::Archive in(std::span<const std::uint8_t>(out), 1);
    in.fixedBlob(four);
    CHECK_FALSE(in.ok());
    CHECK(four == std::vector<std::uint8_t>(4, 0));
}

TEST_CASE("Save -> run N frames == load -> run N frames", "[state]") {
    Machine m = bootMachine();
    runFrames(m, 30);
    const std::uint64_t atSave = m.stateHash();
    const std::vector<std::uint8_t> saved = m.saveState(1234);
    const Snapshot first = runFrames(m, 60);
    CHECK(m.stateHash() != atSave);

    std::string error;
    REQUIRE(m.loadState(saved, error));
    CHECK(m.stateHash() == atSave);
    CHECK(m.frame() == 30);
    const Snapshot again = runFrames(m, 60);
    CHECK(again.state == first.state);
    CHECK(again.frame == first.frame);

    // A freshly powered-on machine of the same ROM continues identically too.
    Machine fresh = bootMachine();
    REQUIRE(fresh.loadState(saved, error));
    CHECK(fresh.stateHash() == atSave);
    CHECK(fresh.vdp().frameHash() != 0);
    const Snapshot other = runFrames(fresh, 60);
    CHECK(other.state == first.state);
    CHECK(other.frame == first.frame);

    // The INFO chunk describes the state.
    const auto info = state::peekStateInfo(saved, error);
    REQUIRE(info);
    CHECK(info->frame == 30);
    CHECK(info->timestamp == 1234);
    CHECK(info->stateHash == atSave);
    CHECK(info->romSha1 == m.romSha1());
}

TEST_CASE("Saving is reproducible and restores the visible frame", "[state]") {
    Machine m = bootMachine();
    runFrames(m, 12);
    const auto a = m.saveState();
    const auto b = m.saveState();
    CHECK(a == b);
    const std::uint64_t frame = m.vdp().frameHash();
    Machine fresh = bootMachine();
    std::string error;
    REQUIRE(fresh.loadState(a, error));
    CHECK(fresh.vdp().frameHash() == frame);
    CHECK(fresh.saveState() == a);
}

TEST_CASE("A state from another ROM is refused and changes nothing", "[state]") {
    Machine m = bootMachine();
    runFrames(m, 5);
    const auto saved = m.saveState();

    test::SyntheticRomOptions opt;
    opt.seed = 7;
    Machine other(test::makeSyntheticRom(opt), cart::kRegionAmericas);
    other.run(RunLimit::frames(2));
    const std::uint64_t before = other.stateHash();
    std::string error;
    CHECK_FALSE(other.loadState(saved, error));
    CHECK_THAT(error, ContainsSubstring("another ROM"));
    CHECK(other.stateHash() == before);
}

TEST_CASE("Chunk versions: newer, unknown, missing and corrupt chunks are refused", "[state]") {
    Machine m = bootMachine();
    runFrames(m, 8);
    const auto saved = m.saveState();
    runFrames(m, 3);
    const std::uint64_t before = m.stateHash();
    std::string error;

    SECTION("the current version of every chunk loads") {
        CHECK(m.loadState(saved, error));
    }
    SECTION("a newer chunk version") {
        const auto bytes = edit(saved, [](state::StateFile& f) { f.find("VDP ")->version = 2; });
        CHECK_FALSE(m.loadState(bytes, error));
        CHECK_THAT(error, ContainsSubstring("'VDP '") && ContainsSubstring("version 2"));
    }
    SECTION("a newer INFO version") {
        const auto bytes = edit(saved, [](state::StateFile& f) { f.find("INFO")->version = 9; });
        CHECK_FALSE(m.loadState(bytes, error));
        CHECK_THAT(error, ContainsSubstring("INFO"));
    }
    SECTION("an unknown chunk") {
        const auto bytes =
            edit(saved, [](state::StateFile& f) { f.chunks.push_back({"XTRA", 1, {1, 2, 3}}); });
        CHECK_FALSE(m.loadState(bytes, error));
        CHECK_THAT(error, ContainsSubstring("'XTRA'") && ContainsSubstring("not understood"));
    }
    SECTION("a missing chunk") {
        const auto bytes = edit(saved, [](state::StateFile& f) {
            std::erase_if(f.chunks, [](const auto& c) { return c.tag == "SND "; });
        });
        CHECK_FALSE(m.loadState(bytes, error));
        CHECK_THAT(error, ContainsSubstring("'SND '"));
    }
    SECTION("a chunk with the wrong size") {
        const auto bytes = edit(saved, [](state::StateFile& f) { f.find("Z80C")->payload.pop_back(); });
        CHECK_FALSE(m.loadState(bytes, error));
        CHECK_THAT(error, ContainsSubstring("'Z80C'") && ContainsSubstring("corrupt"));
    }
    SECTION("changed contents no longer match the recorded state hash") {
        const auto bytes = edit(saved, [](state::StateFile& f) { f.find("BUS ")->payload[100] ^= 0xFF; });
        CHECK_FALSE(m.loadState(bytes, error));
        CHECK_THAT(error, ContainsSubstring("state hash"));
    }
    SECTION("a truncated file") {
        auto bytes = saved;
        bytes.resize(bytes.size() - 10);
        CHECK_FALSE(m.loadState(bytes, error));
        CHECK_THAT(error, ContainsSubstring("truncated"));
    }
    SECTION("a newer container format") {
        auto bytes = saved;
        bytes[8] = 2;
        CHECK_FALSE(m.loadState(bytes, error));
        CHECK_THAT(error, ContainsSubstring("format version 2"));
    }
    SECTION("not a state file") {
        const std::vector<std::uint8_t> bytes(64, 0x42);
        CHECK_FALSE(m.loadState(bytes, error));
        CHECK_THAT(error, ContainsSubstring("not a genesisEmu save state"));
    }
    if (!error.empty()) {
        CHECK(m.stateHash() == before);  // every refusal leaves the machine untouched
    }
}

TEST_CASE("Battery SRAM and its control register are part of a state", "[state]") {
    test::SyntheticRomOptions opt;
    opt.declareSram = true;
    opt.sramStart = 0x200001;
    opt.sramEnd = 0x2001FF;
    Machine m(test::makeSyntheticRom(opt), cart::kRegionAmericas);
    REQUIRE(m.sram() != nullptr);
    cart::SramDevice& sram = *m.sram();
    sram.timeWrite(0xF1, 0x01);
    REQUIRE(sram.memWrite(0x200001, 0x5A));
    sram.clearDirty();
    const auto saved = m.saveState();
    REQUIRE(sram.memWrite(0x200001, 0x00));
    sram.timeWrite(0xF1, 0x03);
    sram.clearDirty();
    std::string error;
    REQUIRE(m.loadState(saved, error));
    std::uint8_t v = 0;
    REQUIRE(sram.memRead(0x200001, v));
    CHECK(v == 0x5A);
    CHECK(sram.control() == 0x01);
    CHECK(sram.dirty());  // the battery file follows the loaded contents
}

TEST_CASE("Thumbnails: optional THMB chunk, ignored by loading", "[state]") {
    Machine m = bootMachine();
    m.run(RunLimit::frames(5));
    const std::vector<std::uint8_t> plain = m.saveState(0);
    const std::vector<std::uint8_t> withThumb = m.saveState(0, true);
    CHECK_FALSE(state::peekThumbnail(plain));
    const auto thumb = state::peekThumbnail(withThumb);
    REQUIRE(thumb);
    CHECK(thumb->width == state::kThumbnailWidth);
    CHECK(thumb->height == state::kThumbnailHeight);
    REQUIRE(thumb->rgba.size() == 160u * 112u);
    // Same picture as the framebuffer (nearest neighbour: every other pixel and line in H40 / 224 lines).
    CHECK((thumb->rgba[0] & 0xFFFFFF) == (m.vdp().framebuffer()[0] & 0xFFFFFF));
    CHECK((thumb->rgba[50 * 160 + 80] & 0xFFFFFF) ==
          (m.vdp().framebuffer()[100 * vdp::kFbWidth + 160] & 0xFFFFFF));
    // Loading ignores it; a state with a thumbnail restores the same machine.
    const std::uint64_t hash = m.stateHash();
    m.run(RunLimit::frames(3));
    std::string error;
    REQUIRE(m.loadState(withThumb, error));
    CHECK(m.stateHash() == hash);
    // A corrupt thumbnail is just "no thumbnail".
    auto file = state::decodeStateFile(withThumb, error);
    REQUIRE(file);
    file->find("THMB")->payload.resize(10);
    CHECK_FALSE(state::peekThumbnail(state::encodeStateFile(*file)));
}
