// Master-clock scheduler, run limits and the synthetic boot ROM (stories M02-04, M02-05, M02-07).

#include "core/sched/machine.hpp"

#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "core/cart/rom.hpp"
#include "support/synthetic_rom.hpp"

using namespace gen;

namespace {

Machine bootMachine(MachineConfig config = {}) {
    return Machine(test::makeBootTestRom(), cart::kRegionAmericas, config);
}

std::uint32_t ramLong(const Machine& m, std::uint32_t addr) {
    const auto& ram = m.bus().state().ram;
    const std::size_t a = addr & 0xFFFF;
    return (static_cast<std::uint32_t>(ram[a]) << 24) | (ram[a + 1] << 16) | (ram[a + 2] << 8) | ram[a + 3];
}

}  // namespace

TEST_CASE("One NTSC frame is 896,040 master clocks and run(frames) stops at the frame end", "[sched]") {
    STATIC_REQUIRE(kMclkPerFrameNtsc == 896040);
    Machine m = bootMachine();
    CHECK(m.frame() == 0);
    CHECK(m.line() == 0);
    const RunResult r = m.run(RunLimit::frames(1));
    CHECK(r.reason == StopReason::LimitReached);
    CHECK(m.frame() == 1);
    CHECK(m.line() == 0);
    CHECK(m.lineStartMclk() == 896040);
    // The 68k may overshoot the boundary by less than one instruction; the overshoot carries over.
    CHECK(m.cpuMclk() >= 896040);
    CHECK(m.cpuMclk() < 896040 + 200 * kM68kDivider);
    m.run(RunLimit::frames(2));
    CHECK(m.frame() == 3);
    CHECK(m.lineStartMclk() == 3 * 896040);
}

TEST_CASE("Line counter advances every 3420 master clocks; PAL frames have 313 lines", "[sched]") {
    Machine m = bootMachine();
    m.run(RunLimit::mclk(kMclkPerLine * 10));
    CHECK(m.line() == 10);
    CHECK(m.vdp().state().line == 10);
    MachineConfig pal;
    pal.region = bus::Region::Europe;
    Machine p = bootMachine(pal);
    p.run(RunLimit::frames(1));
    CHECK(p.lineStartMclk() == static_cast<std::uint64_t>(kMclkPerLine) * kLinesPal);
}

TEST_CASE("Instruction limits, single steps and breakpoints", "[sched]") {
    Machine m = bootMachine();
    CHECK(m.run(RunLimit::instructions(5)).instructions == 5);
    const std::uint32_t pc = m.cpu().state().pc;
    CHECK(m.step(CpuId::M68k).instructions == 1);
    CHECK(m.cpu().state().pc != pc);
    CHECK(m.step(CpuId::Z80).instructions == 0);  // held in reset since power on

    struct Ctx {
        std::uint32_t target;
        int hits = 0;
    } ctx{test::kBootVintHandler};
    Hooks hooks;
    hooks.ctx = &ctx;
    hooks.onInstr = [](void* c, CpuId, std::uint32_t at, std::uint16_t) {
        auto& x = *static_cast<Ctx*>(c);
        if (at == x.target) {
            ++x.hits;
            return Action::Break;
        }
        return Action::Continue;
    };
    m.setHooks(hooks);
    const RunResult r = m.run(RunLimit::untilBreak());
    CHECK(r.reason == StopReason::Breakpoint);
    CHECK(m.cpu().state().pc == test::kBootVintHandler);
    CHECK(ctx.hits == 1);
    // Resumable: the next run executes the instruction and stops at the next V-int.
    CHECK(m.run(RunLimit::untilBreak()).reason == StopReason::Breakpoint);
    CHECK(ctx.hits == 2);
}

TEST_CASE("Boot ROM: Z80 upload, VDP memories, one V-int per frame, wait loop", "[sched][boot]") {
    Machine m = bootMachine();
    std::uint64_t frameEnds = 0;
    Hooks hooks;
    hooks.ctx = &frameEnds;
    hooks.onFrameEnd = [](void* c, std::uint64_t) { ++*static_cast<std::uint64_t*>(c); };
    m.setHooks(hooks);
    m.run(RunLimit::frames(2));
    for (int f = 0; f < 60; ++f) {
        m.run(RunLimit::frames(1));
        REQUIRE(m.stats().vintsLastFrame == 1);
        // The V-int interrupts the two-instruction wait loop (tst.b / bne.s).
        const std::uint32_t at = m.stats().lastVintPc;
        CHECK(at >= 0x200);
        CHECK(at < 0x300);
    }
    CHECK(frameEnds == 62);
    CHECK(m.stats().faults == 0);
    CHECK(ramLong(m, 0xFFF004) >= 60);  // V-int handler counter
    const auto& z = m.z80().state();
    CHECK(z.ram[0] == 0xF3);
    CHECK(z.ram[1] == 0xC3);
    CHECK_FALSE(z.busRequested);
    CHECK_FALSE(z.resetAsserted);
    const auto& v = m.vdp().state();
    CHECK(v.vram[0x20] == 0x00);
    CHECK(v.vram[0x21] == 0x0F);
    CHECK(v.vram[0x3F] == 0x00);
    CHECK(v.cram[0] == 0x0EEE);
    CHECK(m.bus().issues().empty());
}

TEST_CASE("TMSS model: the boot ROM unlocks the VDP itself", "[sched][boot]") {
    MachineConfig config;
    config.tmss = true;
    Machine m = bootMachine(config);
    m.run(RunLimit::frames(3));
    CHECK_FALSE(m.bus().vdpLocked());
    CHECK(m.bus().issueCount(bus::BusIssue::VdpLocked) == 0);
    CHECK(m.stats().vints >= 1);
}

TEST_CASE("State hash is deterministic and covers the running state", "[sched][determinism]") {
    Machine a = bootMachine();
    Machine b = bootMachine();
    CHECK(a.stateHash() == b.stateHash());
    a.run(RunLimit::frames(30));
    const std::uint64_t h30 = a.stateHash();
    b.run(RunLimit::frames(30));
    CHECK(b.stateHash() == h30);
    a.run(RunLimit::frames(1));
    CHECK(a.stateHash() != h30);
    b.hardReset();
    b.run(RunLimit::frames(30));
    CHECK(b.stateHash() == h30);
}

TEST_CASE("Fault exceptions are counted and can stop the run", "[sched]") {
    // A ROM of random bytes faults almost immediately.
    MachineConfig config;
    config.stopOnFault = true;
    Machine m(test::makeSyntheticRom(), cart::kRegionAmericas, config);
    const RunResult r = m.run(RunLimit::frames(10));
    CHECK(r.reason == StopReason::Exception);
    CHECK(isFaultVector(r.vector));
    CHECK(m.stats().faults == 1);
    CHECK(m.stats().firstFaultVector == r.vector);
    CHECK_FALSE(isFaultVector(30));
    CHECK(isFaultVector(3));
}

TEST_CASE("An H-int at the end of the last active line is taken before the V-int", "[sched][irq]") {
    // M06-00b: Sonic 2 sets the H-int counter to 223 when the water is below the screen. The H-int that fires
    // at the end of line 223 must be serviced before the V-int of line 224, or its palette swap lands in
    // V-blank after the V-int's palette upload and tints the whole next frame.
    struct Irq {
        int level = 0;
        std::uint64_t frame = 0;
        int line = 0;
    };
    struct Log {
        std::vector<Irq> irqs;
        const Machine* m = nullptr;
    } log;
    Machine m = bootMachine();
    m.run(RunLimit::frames(3));    // past the boot code; interrupts enabled
    m.vdp().writeControl(0x8014);  // reg 0: H-int enabled
    m.vdp().writeControl(0x8ADF);  // reg 10: 223, loaded into the counter in the next V-blank
    m.run(RunLimit::frames(1));
    log.m = &m;
    Hooks hooks;
    hooks.ctx = &log;
    hooks.onIrq = [](void* ctx, CpuId, int level) {
        auto& l = *static_cast<Log*>(ctx);
        l.irqs.push_back({level, l.m->frame(), l.m->line()});
    };
    m.setHooks(hooks);
    m.run(RunLimit::frames(3));
    // Per frame: exactly one H-int (counter 223) and one V-int, the H-int first, both early in line 224.
    REQUIRE(log.irqs.size() == 6);
    for (std::size_t i = 0; i < log.irqs.size(); i += 2) {
        const Irq& h = log.irqs[i];
        const Irq& v = log.irqs[i + 1];
        CHECK(h.level == 4);
        CHECK(v.level == 6);
        CHECK(h.frame == v.frame);
        CHECK(h.line == 224);
        CHECK(v.line == 224);
    }
}
