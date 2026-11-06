// 68000 core behavior the SingleStepTests suite does not cover: reset, interrupts, STOP, privilege, trace,
// hooks, the TAS quirk and the decode table (stories M01-01, M01-06, M01-08, M01-09, M01-10).

#include "core/m68k/m68k.hpp"

#include <chrono>
#include <cstdint>
#include <initializer_list>
#include <vector>

#include <catch2/catch_test_macros.hpp>

using namespace gen;
using namespace gen::m68k;

namespace {

// Flat 16 MB test bus.
class TestBus final : public Bus68k {
public:
    std::vector<std::uint8_t> mem = std::vector<std::uint8_t>(0x1000000, 0);
    int resets = 0;

    std::uint8_t read8(std::uint32_t a) override { return mem[a]; }
    std::uint16_t read16(std::uint32_t a) override {
        return static_cast<std::uint16_t>((mem[a] << 8) | mem[a + 1]);
    }
    void write8(std::uint32_t a, std::uint8_t v) override { mem[a] = v; }
    void write16(std::uint32_t a, std::uint16_t v) override {
        mem[a] = static_cast<std::uint8_t>(v >> 8);
        mem[a + 1] = static_cast<std::uint8_t>(v);
    }
    void resetDevices() override { ++resets; }

    void put16(std::uint32_t a, std::uint16_t v) { write16(a, v); }
    void put32(std::uint32_t a, std::uint32_t v) {
        write16(a, static_cast<std::uint16_t>(v >> 16));
        write16(a + 2, static_cast<std::uint16_t>(v));
    }
    std::uint16_t get16(std::uint32_t a) { return read16(a); }
    std::uint32_t get32(std::uint32_t a) {
        return (static_cast<std::uint32_t>(read16(a)) << 16) | read16(a + 2);
    }
    void code(std::uint32_t a, std::initializer_list<std::uint16_t> words) {
        for (const std::uint16_t w : words) {
            put16(a, w);
            a += 2;
        }
    }
};

constexpr std::uint32_t kStack = 0x8000;
constexpr std::uint32_t kStart = 0x1000;

// Vector table: SSP, reset PC, and every other vector pointing at 0x4000 + 4 * vector (a NOP there).
void setupVectors(TestBus& bus) {
    bus.put32(0, kStack);
    bus.put32(4, kStart);
    for (std::uint32_t v = 2; v < 64; ++v) {
        bus.put32(v * 4, 0x4000 + v * 4);
        bus.put16(0x4000 + v * 4, 0x4E71);
    }
}

std::uint32_t handlerFor(int vector) {
    return 0x4000 + static_cast<std::uint32_t>(vector) * 4;
}

struct Rig {
    TestBus bus;
    M68k cpu{bus};
    Rig() {
        setupVectors(bus);
        bus.code(kStart, {0x4E71, 0x4E71, 0x4E71, 0x4E71});
    }
    void boot() { cpu.reset(); }
};

}  // namespace

TEST_CASE("Decode table builds quickly and marks invalid opcodes", "[m68k]") {
    const auto start = std::chrono::steady_clock::now();
    const int valid = buildDecodeTableOnce();
    const auto ms =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
#ifdef NDEBUG
    CHECK(ms < 50.0);
#else
    CHECK(ms < 1000.0);  // unoptimized builds are slower; the 50 ms budget applies to release builds
#endif
    CHECK(valid > 40000);
    CHECK(valid < 65536);
    CHECK(isValidOpcode(0x4E71));        // NOP
    CHECK(isValidOpcode(0x2040));        // MOVEA.L D0,A0
    CHECK_FALSE(isValidOpcode(0x4AFC));  // ILLEGAL
    CHECK_FALSE(isValidOpcode(0x4E7B));  // MOVEC (68010+)
    CHECK_FALSE(isValidOpcode(0xA000));  // line A
    CHECK_FALSE(isValidOpcode(0xF000));  // line F
    CHECK_FALSE(isValidOpcode(0x1048));  // MOVE.B A0,D0 (no byte access to An)
    CHECK_FALSE(isValidOpcode(0x15C0));  // MOVE.B D0,d16(PC): PC-relative is not alterable
    CHECK(isValidOpcode(0x13FA));        // MOVE.B d16(PC),abs.L
}

TEST_CASE("Reset loads SSP from $0 and PC from $4", "[m68k]") {
    Rig r;
    CHECK(r.cpu.reset() == 40);
    const State& s = r.cpu.state();
    CHECK(s.a[7] == kStack);
    CHECK(s.pc == kStart);
    CHECK(s.sr == 0x2700);
    CHECK(s.ir == 0x4E71);
    CHECK(r.cpu.lastFlow() == Flow::Exception);
}

TEST_CASE("Unimplemented opcodes take the illegal-instruction exception", "[m68k]") {
    Rig r;
    r.bus.code(kStart, {0x4E7B});  // MOVEC: not a 68000 instruction
    r.boot();
    CHECK(r.cpu.step() == 34);
    const State& s = r.cpu.state();
    CHECK(s.pc == handlerFor(4));
    CHECK(s.a[7] == kStack - 6);
    CHECK(r.bus.get32(kStack - 4) == kStart);  // stacked PC = the illegal instruction
    CHECK(r.cpu.lastFlow() == Flow::Exception);

    Rig a;
    a.bus.code(kStart, {0xA123});
    a.boot();
    a.cpu.step();
    CHECK(a.cpu.state().pc == handlerFor(10));
    Rig f;
    f.bus.code(kStart, {0xF123});
    f.boot();
    f.cpu.step();
    CHECK(f.cpu.state().pc == handlerFor(11));
}

TEST_CASE("Autovectored interrupts respect the mask", "[m68k]") {
    Rig r;
    r.boot();
    r.cpu.state().sr = 0x2300;  // mask 3
    r.cpu.setIrqLevel(6);
    CHECK(r.cpu.step() == 44);
    const State& s = r.cpu.state();
    CHECK(s.pc == r.bus.get32(0x78));  // level 6 autovector is at $78
    CHECK(((s.sr >> 8) & 7) == 6);
    CHECK(r.bus.get32(s.a[7] + 2) == kStart);  // return address
    CHECK(r.bus.get16(s.a[7]) == 0x2300);      // stacked SR

    Rig m;
    m.boot();
    m.cpu.state().sr = 0x2600;  // mask 6: level 6 waits
    m.cpu.setIrqLevel(6);
    m.cpu.step();
    CHECK(m.cpu.state().pc == kStart + 2);  // the NOP ran instead
    m.cpu.state().sr = 0x2500;
    m.cpu.step();
    CHECK(m.cpu.state().pc == m.bus.get32(0x78));
}

TEST_CASE("Level 7 is edge triggered and not maskable", "[m68k]") {
    Rig r;
    r.boot();  // mask 7
    r.cpu.setIrqLevel(7);
    r.cpu.step();
    CHECK(r.cpu.state().pc == r.bus.get32(0x7C));
    r.cpu.step();  // the NOP at the handler; level 7 held but not re-triggered
    CHECK(r.cpu.state().pc == r.bus.get32(0x7C) + 2);
}

TEST_CASE("STOP waits for an interrupt above the mask", "[m68k]") {
    Rig r;
    r.bus.code(kStart, {0x4E72, 0x2300, 0x4E71});  // STOP #$2300
    r.boot();
    CHECK(r.cpu.step() == 4);
    CHECK(r.cpu.state().stopped);
    CHECK(r.cpu.state().pc == kStart + 4);
    r.cpu.setIrqLevel(2);  // below the new mask: still stopped
    r.cpu.step();
    CHECK(r.cpu.state().stopped);
    CHECK(r.cpu.lastFlow() == Flow::Stopped);
    r.cpu.setIrqLevel(4);
    r.cpu.step();
    CHECK_FALSE(r.cpu.state().stopped);
    CHECK(r.cpu.state().pc == r.bus.get32(0x70));
    CHECK(r.bus.get32(r.cpu.state().a[7] + 2) == kStart + 4);  // resumes after STOP
}

TEST_CASE("Privileged instructions trap in user mode and stacks switch", "[m68k]") {
    Rig r;
    r.bus.code(kStart, {0x46FC, 0x0000});      // MOVE #$0000,SR: drop to user mode
    r.bus.code(kStart + 4, {0x46FC, 0x2700});  // privileged in user mode
    r.boot();
    r.cpu.state().usp = 0x6000;
    r.cpu.step();
    State& s = r.cpu.state();
    CHECK_FALSE(s.supervisor());
    CHECK(s.a[7] == 0x6000);
    CHECK(s.supervisorSp() == kStack);
    r.cpu.step();
    CHECK(s.supervisor());
    CHECK(s.pc == handlerFor(8));
    CHECK(s.a[7] == kStack - 6);
    CHECK(s.userSp() == 0x6000);
    CHECK(r.bus.get32(kStack - 4) == kStart + 4);  // stacked PC = the privileged instruction
}

TEST_CASE("Trace exception follows each instruction while T is set", "[m68k]") {
    Rig r;
    r.boot();
    r.cpu.state().sr = 0xA700;
    const int cycles = r.cpu.step();  // NOP, then trace
    CHECK(cycles == 4 + 34);
    CHECK(r.cpu.state().pc == handlerFor(9));
    CHECK(r.bus.get32(r.cpu.state().a[7] + 2) == kStart + 2);
    CHECK((r.cpu.state().sr & kSrTrace) == 0);
}

TEST_CASE("RESET pulses the external reset line", "[m68k]") {
    Rig r;
    r.bus.code(kStart, {0x4E70});
    r.boot();
    CHECK(r.cpu.step() == 132);
    CHECK(r.bus.resets == 1);
}

TEST_CASE("TAS write-back is configurable (Genesis quirk)", "[m68k]") {
    for (const bool writeback : {false, true}) {
        Rig r;
        r.bus.code(kStart, {0x4AD0});  // TAS (A0)
        r.boot();
        r.cpu.setTasWriteback(writeback);
        r.cpu.state().a[0] = 0x2000;
        r.bus.mem[0x2000] = 0x00;
        CHECK(r.cpu.step() == 14);  // the write cycle takes its time even when ignored
        CHECK((r.cpu.state().sr & kSrZero) != 0);
        CHECK(r.bus.mem[0x2000] == (writeback ? 0x80 : 0x00));
    }
}

TEST_CASE("Hooks see every access with the instruction's PC and can break", "[m68k]") {
    struct Log {
        std::vector<std::uint32_t> accessPcs;
        std::vector<AccessKind> kinds;
        int instrs = 0;
        bool breakNext = false;
    } log;
    Hooks hooks;
    hooks.ctx = &log;
    hooks.onInstr = [](void* ctx, CpuId, std::uint32_t, std::uint16_t) {
        auto* l = static_cast<Log*>(ctx);
        ++l->instrs;
        return l->breakNext ? Action::Break : Action::Continue;
    };
    hooks.onAccess = [](void* ctx, CpuId, AccessKind kind, std::uint32_t, std::uint8_t, std::uint32_t,
                        std::uint32_t pc) {
        auto* l = static_cast<Log*>(ctx);
        l->accessPcs.push_back(pc);
        l->kinds.push_back(kind);
    };

    Rig r;
    r.bus.code(kStart, {0x20C0, 0x4E71});  // MOVE.L D0,(A0)+ ; NOP
    r.boot();
    r.cpu.setHooks(hooks);
    r.cpu.state().a[0] = 0x2000;
    r.cpu.step();
    CHECK(log.instrs == 1);
    REQUIRE(log.accessPcs.size() == 3);  // two word writes + one prefetch
    for (const std::uint32_t pc : log.accessPcs) {
        CHECK(pc == kStart);
    }
    CHECK(log.kinds[0] == AccessKind::Write);
    CHECK(log.kinds[2] == AccessKind::Fetch);

    log.breakNext = true;
    CHECK(r.cpu.step() == 0);
    CHECK(r.cpu.breakHit());
    CHECK(r.cpu.state().pc == kStart + 2);  // stopped before the NOP
    log.breakNext = true;
    const int seen = log.instrs;
    CHECK(r.cpu.step() == 4);  // resuming runs it: the hook sees it again, but cannot break it twice
    CHECK(log.instrs == seen + 1);
    CHECK_FALSE(r.cpu.breakHit());
    CHECK(r.cpu.state().pc == kStart + 4);
}

TEST_CASE("Flow type is reported for control transfers", "[m68k]") {
    Rig r;
    // BSR.B +4 ; NOP ; (target) RTS
    r.bus.code(kStart, {0x6102, 0x4E71, 0x4E75});
    r.boot();
    r.cpu.step();
    CHECK(r.cpu.lastFlow() == Flow::Call);
    CHECK(r.cpu.state().pc == kStart + 4);
    r.cpu.step();
    CHECK(r.cpu.lastFlow() == Flow::Return);
    CHECK(r.cpu.state().pc == kStart + 2);
    r.cpu.step();
    CHECK(r.cpu.lastFlow() == Flow::Sequential);
}

TEST_CASE("Odd word access raises an address error with a group 0 frame", "[m68k]") {
    Rig r;
    r.bus.code(kStart, {0x3010});  // MOVE.W (A0),D0
    r.boot();
    r.cpu.state().a[0] = 0x2001;
    CHECK(r.cpu.step() == 50);
    const State& s = r.cpu.state();
    CHECK(s.pc == handlerFor(3));
    CHECK(s.a[7] == kStack - 14);
    CHECK(r.bus.get32(s.a[7] + 2) == 0x2001);     // access address
    CHECK(r.bus.get16(s.a[7] + 6) == 0x3010);     // instruction register
    CHECK((r.bus.get16(s.a[7]) & 0x1F) == 0x15);  // read, data, supervisor data
}
