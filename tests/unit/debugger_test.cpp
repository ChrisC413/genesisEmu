// Debugger core (M06-01..04, 06, 09, 10, 11): run control, stepping, breakpoints and conditions,
// watchpoints, trace from pause, ring buffer, call stack, memory search and the disassembly listing, against
// a small hand-assembled program (original code; no commercial data).

#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <sstream>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "core/cart/rom.hpp"
#include "core/sched/machine.hpp"
#include "debug/debugger.hpp"
#include "debug/expr.hpp"
#include "debug/listing.hpp"
#include "debug/memory.hpp"
#include "support/synthetic_rom.hpp"

using namespace gen;
using namespace gen::debug;

namespace {

class Emitter {
public:
    Emitter(std::vector<std::uint8_t>& rom, std::uint32_t at) : rom_(rom), pc_(at) {}
    void w(std::initializer_list<std::uint16_t> words) {
        for (const std::uint16_t v : words) {
            rom_[pc_++] = static_cast<std::uint8_t>(v >> 8);
            rom_[pc_++] = static_cast<std::uint8_t>(v);
        }
    }
    void b(std::initializer_list<std::uint8_t> bytes) {
        for (const std::uint8_t v : bytes) {
            rom_[pc_++] = v;
        }
    }
    void branch(std::uint16_t op, std::uint32_t target) {  // Bcc.s
        const auto disp = static_cast<std::int32_t>(target) - static_cast<std::int32_t>(pc_ + 2);
        w({static_cast<std::uint16_t>(op | (static_cast<std::uint32_t>(disp) & 0xFF))});
    }
    std::uint32_t here() const { return pc_; }

private:
    std::vector<std::uint8_t>& rom_;
    std::uint32_t pc_;
};

void putLong(std::vector<std::uint8_t>& rom, std::uint32_t at, std::uint32_t v) {
    for (std::uint32_t i = 0; i < 4; ++i) {
        rom[at + i] = static_cast<std::uint8_t>(v >> (24 - 8 * i));
    }
}

// Program layout (addresses fixed so the tests can name them).
constexpr std::uint32_t kFault = 0x300;
constexpr std::uint32_t kVint = 0x340;
constexpr std::uint32_t kSub = 0x400;         // move.w #$10,d0 ; bsr.w inner ; move.b d0,($F010).w ; rts
constexpr std::uint32_t kSubBsr = 0x404;
constexpr std::uint32_t kSubAfterBsr = 0x408;
constexpr std::uint32_t kSubRts = 0x40C;
constexpr std::uint32_t kInner = 0x420;       // waits for the next V-int, then rts
constexpr std::uint32_t kInnerWait = 0x424;
constexpr std::uint32_t kZ80Code = 0x700;
constexpr std::uint32_t kCramData = 0x600;

struct Program {
    std::vector<std::uint8_t> rom;
    std::uint32_t main = 0;       // jsr (kSub).l
    std::uint32_t afterJsr = 0;   // addq.w #1,($F002).w
};

Program makeProgram() {
    test::SyntheticRomOptions options;
    options.domesticTitle = "DEBUGGER TEST ROM";
    options.overseasTitle = "DEBUGGER TEST ROM";
    options.regionCodes = "U";
    Program p;
    p.rom = test::makeSyntheticRom(options);
    std::vector<std::uint8_t>& rom = p.rom;
    for (std::uint32_t v = 2; v < 64; ++v) {
        putLong(rom, v * 4, kFault);
    }
    putLong(rom, 28 * 4, kFault + 0x20);
    putLong(rom, 30 * 4, kVint);

    Emitter e(rom, 0x200);
    e.w({0x46FC, 0x2700});                  // move.w #$2700,sr
    e.w({0x33FC, 0x0100, 0x00A1, 0x1100});  // Z80 bus request
    e.w({0x33FC, 0x0100, 0x00A1, 0x1200});  // Z80 reset released
    const std::uint32_t waitBus = e.here();
    e.w({0x0839, 0x0000, 0x00A1, 0x1100});  // btst #0,($A11100).l
    e.branch(0x6600, waitBus);
    e.w({0x41F9, 0x0000, static_cast<std::uint16_t>(kZ80Code)});  // lea (kZ80Code).l,a0
    e.w({0x47F9, 0x00A0, 0x0000});                                // lea ($A00000).l,a3
    e.w({0x7411});                                                // moveq #17,d2
    const std::uint32_t copy = e.here();
    e.w({0x16D8});                                                      // move.b (a0)+,(a3)+
    e.w({0x51CA, static_cast<std::uint16_t>(copy - (e.here() + 2))});  // dbf d2,copy
    e.w({0x33FC, 0x0000, 0x00A1, 0x1200});                              // Z80 reset
    e.w({0x33FC, 0x0000, 0x00A1, 0x1100});                              // release the bus
    e.w({0x33FC, 0x0100, 0x00A1, 0x1200});                              // Z80 runs
    e.w({0x43F9, 0x00C0, 0x0004});                                      // lea ($C00004).l,a1
    e.w({0x45F9, 0x00C0, 0x0000});                                      // lea ($C00000).l,a2
    e.w({0x32BC, 0x8004});                                              // H-int off
    e.w({0x32BC, 0x8174});                                              // display, V-int, DMA on
    e.w({0x32BC, 0x8F02});                                              // auto-increment 2
    e.w({0x22BC, 0xC002, 0x0000});                                      // CRAM write $0002
    e.w({0x34BC, 0x0EEE});                                              // move.w #$0EEE,(a2)
    // 68k -> CRAM DMA of one word from kCramData to CRAM 0.
    e.w({0x32BC, 0x9301, 0x32BC, 0x9400, 0x32BC, 0x9500, 0x32BC, 0x9603, 0x32BC, 0x9700});
    e.w({0x22BC, 0xC000, 0x0080});
    e.w({0x46FC, 0x2000});  // move.w #$2000,sr
    p.main = e.here();
    e.w({0x4EB9, 0x0000, static_cast<std::uint16_t>(kSub)});  // jsr (kSub).l
    p.afterJsr = e.here();
    e.w({0x5278, 0xF002});    // addq.w #1,($F002).w
    e.branch(0x6000, p.main);  // bra.s main

    Emitter fault(rom, kFault);
    fault.w({0x60FE});
    Emitter hint(rom, kFault + 0x20);
    hint.w({0x4E73});
    Emitter vint(rom, kVint);
    vint.w({0x52B8, 0xF004});  // addq.l #1,($F004).w
    vint.w({0x4E73});          // rte

    Emitter sub(rom, kSub);
    sub.w({0x303C, 0x0010});                                            // move.w #$10,d0
    sub.w({0x6100, static_cast<std::uint16_t>(kInner - (kSubBsr + 2))});  // bsr.w inner
    sub.w({0x11C0, 0xF010});                                            // move.b d0,($F010).w
    sub.w({0x4E75});                                                    // rts
    Emitter inner(rom, kInner);
    inner.w({0x2238, 0xF004});  // move.l ($F004).w,d1
    inner.w({0xB2B8, 0xF004});  // cmp.l ($F004).w,d1
    inner.w({0x67FA});          // beq.s wait
    inner.w({0x4E75});          // rts

    Emitter cram(rom, kCramData);
    cram.w({0x0E00});

    // Z80: ld sp,$1FF0 ; ld a,1 ; call $0010 ; ld ($1000),a ; jr $0003 ... $0010: inc a ; ret
    Emitter z(rom, kZ80Code);
    z.b({0x31, 0xF0, 0x1F, 0x3E, 0x01, 0xCD, 0x10, 0x00, 0x32, 0x00, 0x10, 0x18, 0xF6, 0, 0, 0, 0x3C, 0xC9});

    const std::uint16_t sum = cart::computeChecksum(rom);
    rom[0x18E] = static_cast<std::uint8_t>(sum >> 8);
    rom[0x18F] = static_cast<std::uint8_t>(sum);
    return p;
}

Machine makeMachine(const Program& p) {
    return Machine(p.rom, cart::parseHeader(p.rom).regions);
}

// Runs until the 68k is about to execute `pc` (no debugger involved).
void runUntilPc(Machine& m, std::uint32_t pc) {
    for (int i = 0; i < 2000000 && m.cpu().state().pc != pc; ++i) {
        m.step(CpuId::M68k);
    }
    REQUIRE(m.cpu().state().pc == pc);
}

struct RegsCtx final : EvalContext {
    std::int64_t reg(int id) const override { return id == 0 ? 0x12340010 : id == 8 ? 0xFF0000 : 0; }
    std::uint32_t read(std::uint32_t addr, int size) const override {
        return addr == 0xFF0024 && size == 1 ? 7 : 0;
    }
    std::int64_t variable(int id) const override { return id == kVarFrame ? 100 : 0; }
};

std::int64_t eval(const char* text, CpuId cpu = CpuId::M68k) {
    std::string error;
    const auto e = Expr::compile(text, cpu, error);
    INFO(text << ": " << error);
    REQUIRE(e);
    return e->eval(RegsCtx{});
}

}  // namespace

TEST_CASE("Expressions: literals, registers, sizes, memory and precedence", "[debug][expr]") {
    CHECK(eval("$10") == 16);
    CHECK(eval("0x10 + %101 + 3") == 24);
    CHECK(eval("d0.w == $10") == 1);
    CHECK(eval("d0.b") == 0x10);
    CHECK(eval("d0.l") == 0x12340010);
    CHECK(eval("d0 == $12340010 && frame >= 100") == 1);
    CHECK(eval("byte(a0+$24) == 7") == 1);
    CHECK(eval("1 + 2 * 3") == 7);
    CHECK(eval("(1 + 2) * 3") == 9);
    CHECK(eval("1 << 4 | 1") == 17);
    CHECK(eval("!0 + ~0") == 0);
    CHECK(eval("-5 < 3") == 1);
    CHECK(eval("7 % 0") == 0);  // division by zero gives 0 instead of trapping
    CHECK(eval("1 || 0 && 0") == 1);
    CHECK(eval("a == 0", CpuId::Z80) == 1);  // RegsCtx has no Z80 registers

    std::string error;
    CHECK_FALSE(Expr::compile("d0 ==", CpuId::M68k, error));
    CHECK(error.find("column") != std::string::npos);
    CHECK_FALSE(Expr::compile("foo + 1", CpuId::M68k, error));
    CHECK(error.find("foo") != std::string::npos);
    CHECK_FALSE(Expr::compile("ix", CpuId::M68k, error));  // a Z80 register on the 68k
    CHECK_FALSE(Expr::compile("(1", CpuId::M68k, error));
}

TEST_CASE("Logpoint formats", "[debug][expr]") {
    std::string error;
    const auto f = Format::compile("obj {a0:x} routine {byte(a0+$24)} d0={d0.w:X4} {{x}}", CpuId::M68k, error);
    REQUIRE(f);
    CHECK(f->render(RegsCtx{}) == "obj ff0000 routine 7 d0=0010 {x}");
    CHECK_FALSE(Format::compile("{d0", CpuId::M68k, error));
    CHECK_FALSE(Format::compile("{d0:q}", CpuId::M68k, error));
}

TEST_CASE("Run control: line and frame limits, soft reset, stop reasons", "[debug][run]") {
    const Program p = makeProgram();
    Machine m = makeMachine(p);
    m.run(RunLimit::frames(3));
    const int line = m.line();
    const std::uint64_t frame = m.frame();
    REQUIRE(m.run(RunLimit::lines(1)).reason == StopReason::LimitReached);
    CHECK(m.line() == (line + 1) % m.linesPerFrame());
    m.run(RunLimit::lines(static_cast<std::uint64_t>(m.linesPerFrame())));
    CHECK(m.frame() == frame + 1);
    CHECK(m.line() == (line + 1) % m.linesPerFrame());
    // Step frame advances exactly one frame and stops at its first instruction boundary.
    const std::uint64_t f = m.frame();
    m.run(RunLimit::frames(1));
    CHECK(m.frame() == f + 1);
    CHECK(m.line() == 0);

    const std::uint32_t ramBefore = m.bus().state().ram[0xF004 + 3];
    m.softReset();
    CHECK(m.cpu().state().pc == 0x200);
    CHECK(m.bus().state().ram[0xF004 + 3] == ramBefore);  // memory survives a soft reset
    CHECK(m.frame() == f + 1);
}

TEST_CASE("Idle debugger installs no hooks and never changes results", "[debug][run]") {
    const Program p = makeProgram();
    Machine plain = makeMachine(p);
    plain.run(RunLimit::frames(30));

    Machine m = makeMachine(p);
    Debugger d(m);
    CHECK_FALSE(d.hooksInstalled());
    for (int i = 0; i < 10; ++i) {
        d.run(RunLimit::frames(1));
    }
    // Observers on: ring buffer, call stack tracking, a never-matching breakpoint and watchpoint.
    d.setRingEnabled(true, 1000);
    d.setCallStackTracking(true);
    std::string error;
    Breakpoint bp;
    bp.kind = BpKind::Exec;
    bp.start = bp.end = 0x7000;
    REQUIRE(d.add(bp, error) > 0);
    Breakpoint wp;
    wp.kind = BpKind::Write;
    wp.start = wp.end = 0xFF8000;
    REQUIRE(d.add(wp, error) > 0);
    CHECK(d.hooksInstalled());
    for (int i = 0; i < 10; ++i) {
        CHECK(d.run(RunLimit::frames(1)).reason == StopReason::LimitReached);
    }
    d.setRingEnabled(false);
    d.setCallStackTracking(false);
    d.clearBreakpoints();
    CHECK_FALSE(d.hooksInstalled());
    for (int i = 0; i < 10; ++i) {
        d.run(RunLimit::frames(1));
    }
    CHECK(m.stateHash() == plain.stateHash());
    CHECK(m.vdp().frameHash() == plain.vdp().frameHash());
}

TEST_CASE("Execution breakpoints: stop, resume, conditions, hit counts, temp, logpoints", "[debug][bp]") {
    const Program p = makeProgram();
    Machine m = makeMachine(p);
    Debugger d(m);
    std::string error;
    Breakpoint bp;
    bp.kind = BpKind::Exec;
    bp.start = bp.end = kSubAfterBsr;
    const int id = d.add(bp, error);
    REQUIRE(id > 0);
    RunResult r = d.run(RunLimit::frames(10));
    REQUIRE(r.reason == StopReason::Breakpoint);
    CHECK(m.cpu().state().pc == kSubAfterBsr);
    CHECK(d.lastStop().kind == StopInfo::Kind::Breakpoint);
    CHECK(d.lastStop().breakpoint == id);
    CHECK(d.find(id)->hits == 1);
    // Resuming does not stop on the same instruction again; the next pass does.
    const std::uint64_t frame = m.frame();
    r = d.run(RunLimit::frames(10));
    REQUIRE(r.reason == StopReason::Breakpoint);
    CHECK(m.frame() > frame);
    CHECK(d.find(id)->hits == 2);

    // Condition: d0.w == $10 is true at kSubAfterBsr; d0.w == $11 never.
    Breakpoint c = *d.find(id);
    c.condition = "d0.w == $11";
    REQUIRE(d.update(c, error));
    CHECK(d.run(RunLimit::frames(5)).reason == StopReason::LimitReached);
    c.condition = "d0.w == $10 && frame > 0";
    REQUIRE(d.update(c, error));
    CHECK(d.run(RunLimit::frames(5)).reason == StopReason::Breakpoint);
    c.condition = "bogus(";
    CHECK_FALSE(d.update(c, error));

    // Hit count: ignore the first two hits.
    d.clearBreakpoints();
    Breakpoint h;
    h.kind = BpKind::Exec;
    h.start = h.end = kSub;
    h.ignoreHits = 2;
    const int hid = d.add(h, error);
    const std::uint64_t f0 = m.frame();
    REQUIRE(d.run(RunLimit::frames(20)).reason == StopReason::Breakpoint);
    CHECK(d.find(hid)->hits == 3);
    CHECK(m.frame() >= f0 + 2);

    // Temporary breakpoints disappear after stopping.
    d.clearBreakpoints();
    Breakpoint t;
    t.kind = BpKind::Exec;
    t.start = t.end = p.afterJsr;
    t.temporary = true;
    d.add(t, error);
    REQUIRE(d.run(RunLimit::frames(5)).reason == StopReason::Breakpoint);
    CHECK(d.breakpoints().empty());

    // Logpoints log and continue.
    Breakpoint lp;
    lp.kind = BpKind::Exec;
    lp.start = lp.end = kSubAfterBsr;
    lp.log = true;
    lp.stop = false;
    lp.message = "d0={d0.w:x} frame {frame}";
    REQUIRE(d.add(lp, error) > 0);
    CHECK(d.run(RunLimit::frames(3)).reason == StopReason::LimitReached);
    REQUIRE(d.log().size() >= 2);
    CHECK(d.log().front().starts_with("d0=10 frame "));
}

TEST_CASE("Toggle exec breakpoints and 100 active breakpoints keep the O(1) check", "[debug][bp]") {
    const Program p = makeProgram();
    Machine m = makeMachine(p);
    Debugger d(m);
    CHECK(d.toggleExec(CpuId::M68k, 0x1234));
    CHECK(d.hasExec(CpuId::M68k, 0x1234));
    CHECK_FALSE(d.toggleExec(CpuId::M68k, 0x1234));
    CHECK(d.breakpoints().empty());
    std::string error;
    for (std::uint32_t i = 0; i < 100; ++i) {
        Breakpoint bp;
        bp.kind = BpKind::Exec;
        bp.start = bp.end = 0x10000 + i * 2;  // never executed
        d.add(bp, error);
    }
    Machine plain = makeMachine(p);
    plain.run(RunLimit::frames(20));
    CHECK(d.run(RunLimit::frames(20)).reason == StopReason::LimitReached);
    CHECK(m.stateHash() == plain.stateHash());
}

TEST_CASE("Step into, over (with interrupts in between), out and run to cursor", "[debug][step]") {
    const Program p = makeProgram();
    Machine m = makeMachine(p);
    Debugger d(m);
    runUntilPc(m, p.main);

    // Step over the jsr: kSub -> inner waits for a V-int, so interrupts are taken in between.
    const std::uint32_t sp = m.cpu().state().a[7];
    const std::uint64_t vints = m.stats().vints;
    d.stepOver(CpuId::M68k);
    CHECK(m.cpu().state().pc == p.afterJsr);
    CHECK(m.cpu().state().a[7] == sp);
    CHECK(m.stats().vints > vints);
    CHECK(d.lastStop().kind == StopInfo::Kind::Step);
    CHECK_FALSE(d.hooksInstalled());  // the step's hook is gone once it completed

    // Step over a non-call is a step into.
    const std::uint32_t at = m.cpu().state().pc;
    d.stepOver(CpuId::M68k);
    CHECK(m.cpu().state().pc == at + 4);

    // Step into the jsr, then into the bsr; step out returns to kSubAfterBsr, step out again to main.
    runUntilPc(m, p.main);
    d.stepInto(CpuId::M68k);
    CHECK(m.cpu().state().pc == kSub);
    d.stepInto(CpuId::M68k);
    d.stepInto(CpuId::M68k);
    CHECK(m.cpu().state().pc == kInner);
    d.stepOut(CpuId::M68k);
    CHECK(m.cpu().state().pc == kSubAfterBsr);
    d.stepOut(CpuId::M68k);
    CHECK(m.cpu().state().pc == p.afterJsr);
    CHECK(m.cpu().state().a[7] == sp);

    // Run to cursor.
    d.runTo(CpuId::M68k, kSubRts);
    CHECK(m.cpu().state().pc == kSubRts);
    CHECK(d.lastStop().kind == StopInfo::Kind::Step);

    // A breakpoint inside the stepped-over routine wins over the step.
    runUntilPc(m, p.main);
    d.toggleExec(CpuId::M68k, kInnerWait);
    d.stepOver(CpuId::M68k);
    CHECK(m.cpu().state().pc == kInnerWait);
    CHECK(d.lastStop().kind == StopInfo::Kind::Breakpoint);
}

TEST_CASE("Z80 stepping keeps the 68k in sync; Z80 step over and breakpoints", "[debug][step][z80]") {
    const Program p = makeProgram();
    Machine m = makeMachine(p);
    Debugger d(m);
    m.run(RunLimit::frames(2));
    REQUIRE(m.z80().running());
    std::string error;
    // Break on the Z80's "call $0010".
    Breakpoint bp;
    bp.kind = BpKind::Exec;
    bp.cpu = CpuId::Z80;
    bp.start = bp.end = 0x0005;
    REQUIRE(d.add(bp, error) > 0);
    RunResult r = d.run(RunLimit::frames(2));
    REQUIRE(r.reason == StopReason::Breakpoint);
    CHECK(r.cpu == CpuId::Z80);
    CHECK(m.z80Cpu().state().pc == 0x0005);
    d.clearBreakpoints();

    for (int i = 0; i < 4; ++i) {
        const std::uint16_t pc = m.z80Cpu().state().pc;
        const std::uint64_t z80Before = m.z80Mclk();
        const std::uint64_t cpuBefore = m.cpuMclk();
        d.stepInto(CpuId::Z80);
        CHECK(m.z80Cpu().state().pc != pc);
        CHECK(m.z80Mclk() > z80Before);
        CHECK(m.cpuMclk() >= cpuBefore);
        // The two CPUs stay within one line of each other.
        CHECK(m.cpuMclk() + kMclkPerLine >= m.z80Mclk());
        CHECK(m.z80Mclk() + 2 * kMclkPerLine >= m.cpuMclk());
    }
    // Get back to the call and step over it.
    d.runTo(CpuId::Z80, 0x0005);
    REQUIRE(m.z80Cpu().state().pc == 0x0005);
    d.stepOver(CpuId::Z80);
    CHECK(m.z80Cpu().state().pc == 0x0008);
    CHECK(m.z80Cpu().state().a == 2);
    d.runTo(CpuId::Z80, 0x0010);
    d.stepOut(CpuId::Z80);
    CHECK(m.z80Cpu().state().pc == 0x0008);
}

TEST_CASE("Memory watchpoints report the accessing PC; VDP, DMA, interrupt, exception, frame", "[debug][watch]") {
    const Program p = makeProgram();
    Machine m = makeMachine(p);
    Debugger d(m);
    std::string error;
    Breakpoint wp;
    wp.kind = BpKind::Write;
    wp.start = wp.end = 0xFFF010;
    REQUIRE(d.add(wp, error) > 0);
    RunResult r = d.run(RunLimit::frames(5));
    REQUIRE(r.reason == StopReason::Watchpoint);
    CHECK(d.lastStop().pc == kSubAfterBsr);              // move.b d0,($F010).w
    CHECK(m.cpu().state().pc == kSubRts);                // stopped after it
    CHECK(d.lastStop().address == 0xFFF010);
    CHECK(d.lastStop().value == 0x10);
    // Mirror addresses are canonical; value conditions work.
    d.clearBreakpoints();
    wp.start = wp.end = 0xE0F010;
    wp.condition = "value == $11";
    REQUIRE(d.add(wp, error) > 0);
    CHECK(d.run(RunLimit::frames(3)).reason == StopReason::LimitReached);
    d.clearBreakpoints();
    // A read watch on the V-int counter stops in the inner wait loop.
    Breakpoint rd;
    rd.kind = BpKind::Read;
    rd.start = 0xFFF004;
    rd.end = 0xFFF007;
    rd.sizes = 0x2;  // the 68k bus is 16 bits wide: a long access is two word accesses
    d.add(rd, error);
    REQUIRE(d.run(RunLimit::frames(3)).reason == StopReason::Watchpoint);
    CHECK((d.lastStop().pc == kInner || d.lastStop().pc == kInnerWait || d.lastStop().pc == kVint));
    d.clearBreakpoints();

    // VDP: CRAM data-port write and the CRAM DMA during init (fresh machine).
    Machine m2 = makeMachine(p);
    Debugger d2(m2);
    Breakpoint cw;
    cw.kind = BpKind::Cram;
    cw.start = cw.end = 0x0002;
    d2.add(cw, error);
    Breakpoint dma;
    dma.kind = BpKind::Dma;
    dma.dmaTarget = static_cast<int>(vdp::DmaTarget::Cram);
    dma.start = 0;
    dma.end = 0x7F;
    d2.add(dma, error);
    Breakpoint reg;
    reg.kind = BpKind::VdpReg;
    reg.start = reg.end = 1;
    reg.condition = "value & $20";
    d2.add(reg, error);
    REQUIRE(d2.run(RunLimit::frames(2)).reason == StopReason::Watchpoint);
    CHECK(d2.lastStop().text.find("VDP register") != std::string::npos);
    CHECK(d2.lastStop().value == 0x74);
    REQUIRE(d2.run(RunLimit::frames(2)).reason == StopReason::Watchpoint);
    CHECK(d2.lastStop().text.find("CRAM write") != std::string::npos);
    CHECK(d2.lastStop().value == 0x0EEE);
    REQUIRE(d2.run(RunLimit::frames(2)).reason == StopReason::Watchpoint);
    CHECK(d2.lastStop().text.find("DMA") != std::string::npos);
    CHECK(d2.lastStop().value == kCramData);
    d2.clearBreakpoints();

    // Interrupt level 6 stops at the handler's first instruction; so does the exception vector.
    Breakpoint irq;
    irq.kind = BpKind::Interrupt;
    irq.start = irq.end = 6;
    d2.add(irq, error);
    REQUIRE(d2.run(RunLimit::frames(2)).reason == StopReason::Watchpoint);
    CHECK(m2.cpu().state().pc == kVint);
    d2.clearBreakpoints();
    Breakpoint ex;
    ex.kind = BpKind::Exception;
    ex.start = ex.end = 30;
    d2.add(ex, error);
    REQUIRE(d2.run(RunLimit::frames(2)).reason == StopReason::Watchpoint);
    CHECK(m2.cpu().state().pc == kVint);
    CHECK(d2.lastStop().text.find("V-int") != std::string::npos);
    d2.clearBreakpoints();
    Breakpoint fr;
    fr.kind = BpKind::Frame;
    fr.start = fr.end = static_cast<std::uint32_t>(m2.frame() + 3);
    d2.add(fr, error);
    REQUIRE(d2.run(RunLimit::frames(10)).reason == StopReason::Watchpoint);
    CHECK(m2.frame() == fr.start);
    CHECK(m2.line() == 0);

    // Z80 write watch.
    d2.clearBreakpoints();
    Breakpoint zw;
    zw.kind = BpKind::Write;
    zw.cpu = CpuId::Z80;
    zw.start = zw.end = 0x1000;
    d2.add(zw, error);
    REQUIRE(d2.run(RunLimit::frames(2)).reason == StopReason::Watchpoint);
    CHECK(d2.lastStop().cpu == CpuId::Z80);
    CHECK(d2.lastStop().pc == 0x0008);
    CHECK(m2.z80Cpu().state().pc == 0x000B);  // the Z80 froze right after the writing instruction
}

TEST_CASE("Trace from pause: step 10 gives exactly 10 records; ring buffer dump", "[debug][trace]") {
    const Program p = makeProgram();
    Machine m = makeMachine(p);
    Debugger d(m);
    m.run(RunLimit::frames(2));
    const auto path = std::filesystem::temp_directory_path() / "genesisEmu_debugger_test_trace.txt";
    std::string error;
    REQUIRE(d.beginTrace(path.string(), {}, error));
    CHECK(d.hooksInstalled());
    for (int i = 0; i < 10; ++i) {
        d.stepInto(CpuId::M68k);
    }
    d.endTrace();
    CHECK_FALSE(d.hooksInstalled());
    std::ifstream in(path);
    int records = 0;
    for (std::string line; std::getline(in, line);) {
        if (!line.empty() && line[0] != '#') {
            ++records;
        }
    }
    in.close();
    std::filesystem::remove(path);
    CHECK(records == 10);

    // Ring buffer: after a breakpoint the dump ends with the instructions leading into it.
    d.setRingEnabled(true, 64);
    d.toggleExec(CpuId::M68k, kSubAfterBsr);
    REQUIRE(d.run(RunLimit::frames(5)).reason == StopReason::Breakpoint);
    const auto ring = d.ring();
    REQUIRE(ring.size() == 64);
    std::vector<std::uint32_t> pcs68k;
    for (const RingEntry& e : ring) {
        if (e.cpu == CpuId::M68k && e.irq == 0) {
            pcs68k.push_back(e.pc);
        }
    }
    REQUIRE(!pcs68k.empty());
    CHECK(pcs68k.back() == 0x42A);  // inner's rts, the instruction before the breakpoint
    std::ostringstream dump;
    d.dumpRing(dump);
    CHECK(dump.str().find("rts") != std::string::npos);
    CHECK(dump.str().find("bsr.w") == std::string::npos);  // 64 entries reach back only into inner's loop
}

TEST_CASE("Call stack: shadow tracking and stack walk; interrupt frames", "[debug][callstack]") {
    const Program p = makeProgram();
    Machine m = makeMachine(p);
    Debugger d(m);
    runUntilPc(m, p.main);
    d.setCallStackTracking(true);
    d.toggleExec(CpuId::M68k, kVint);
    REQUIRE(d.run(RunLimit::frames(5)).reason == StopReason::Breakpoint);
    // main -> jsr sub -> bsr inner -> V-int
    auto stack = d.callStack();
    REQUIRE(stack.size() == 3);
    CHECK(stack[0].kind == CallFrame::Kind::Call);
    CHECK(stack[0].callPc == p.main);
    CHECK(stack[0].entry == kSub);
    CHECK(stack[1].callPc == kSubBsr);
    CHECK(stack[1].entry == kInner);
    CHECK(stack[1].returnPc == kSubAfterBsr);
    CHECK(stack[2].kind == CallFrame::Kind::Interrupt);
    CHECK(stack[2].vector == 30);
    CHECK(describeVector(stack[2].vector) == "V-int (level 6)");
    CHECK((stack[2].returnPc == kInnerWait || stack[2].returnPc == kInnerWait + 4));
    // After the rte and both returns the stack is empty again.
    d.clearBreakpoints();
    d.runTo(CpuId::M68k, p.afterJsr);
    CHECK(d.callStack().empty());

    // Without tracking, the heuristic walk finds the same two calls from the return addresses.
    d.setCallStackTracking(false);
    d.runTo(CpuId::M68k, kInnerWait);
    const auto walked = d.callStack();
    REQUIRE(walked.size() == 2);
    CHECK(walked[0].callPc == p.main);
    CHECK(walked[0].entry == kSub);
    CHECK(walked[1].callPc == kSubBsr);
    CHECK(walked[1].entry == kInner);
}

TEST_CASE("Memory: pattern search, region edits and ROM patches", "[debug][memory]") {
    std::string error;
    const auto pat = parsePattern("4E 75 ?? 00", error);
    REQUIRE(pat);
    const std::vector<std::uint8_t> data{0x4E, 0x75, 0x12, 0x00, 0x4E, 0x75, 0x4E, 0x75, 0xFF, 0x00};
    CHECK(findAll(data, *pat) == std::vector<std::size_t>{0, 6});
    CHECK(findAll(data, *parsePattern("4e75", error)) == std::vector<std::size_t>{0, 4, 6});
    CHECK_FALSE(parsePattern("4E7", error));
    CHECK_FALSE(parsePattern("4G", error));
    CHECK_FALSE(parsePattern("4?", error));

    const Program p = makeProgram();
    Machine m = makeMachine(p);
    // Every rts in the ROM.
    const auto rts = findAll(m, Region::Rom, *parsePattern("4E75", error));
    CHECK(std::find(rts.begin(), rts.end(), kSubRts) != rts.end());
    CHECK(std::find(rts.begin(), rts.end(), 0x42A) != rts.end());

    // Edit RAM and CRAM; patch the ROM so the subroutine stores $22 instead of d0.
    std::vector<RomPatch> patches;
    REQUIRE(writeRegion(m, Region::Ram68k, 0xF100, 0xAB, patches));
    CHECK(peek(m, CpuId::M68k, 0xFFF100) == 0xAB);
    REQUIRE(writeRegion(m, Region::Cram, 1, 0xFF, patches));
    CHECK(m.vdp().state().cram[0] == 0x00EE);
    REQUIRE(writeRegion(m, Region::Rom, kSub + 3, 0x22, patches));  // move.w #$22,d0
    CHECK(patches.size() == 1);
    m.run(RunLimit::frames(5));
    CHECK(m.bus().state().ram[0xF010] == 0x22);  // the edit is reflected in emulation
    REQUIRE(writeRegion(m, Region::Rom, kSub + 3, 0x10, patches));
    CHECK(patches.empty());  // back to the original value
    CHECK_FALSE(writeRegion(m, Region::Sram, 0, 0, patches));
}

TEST_CASE("Disassembly listing: backward scrolling resynchronizes on instruction starts", "[debug][listing]") {
    const Program p = makeProgram();
    Machine m = makeMachine(p);
    KnownStarts known;
    // Forward from the jsr.
    const auto lines = buildListing(m, CpuId::M68k, p.main, 0, 3, known);
    REQUIRE(lines.size() == 3);
    CHECK(lines[0].text.starts_with("jsr"));
    CHECK(lines[1].addr == p.afterJsr);
    // Backwards: the instruction before the sub's bsr.w is the move.w #$10,d0 (4 bytes), even though
    // $0402 (the immediate) also decodes.
    CHECK(previousStart(m, CpuId::M68k, kSubBsr, known) == kSub);
    // Every line of a listing that ends on the PC is aligned with the real instruction stream.
    known.add(CpuId::M68k, p.main);
    const auto back = buildListing(m, CpuId::M68k, p.afterJsr, 1, 2, known);
    CHECK(back[0].addr == p.main);
    CHECK(back[1].addr == p.afterJsr);
    // Z80.
    const auto z = buildListing(m, CpuId::Z80, 0, 0, 1, known);
    (void)z;  // Z80 RAM is empty before the upload; just must not crash
}

TEST_CASE("TMP bisect", "[tmpbisect]") {
    const Program p = makeProgram();
    Machine plain = makeMachine(p);
    plain.run(RunLimit::frames(30));
    Machine chunks = makeMachine(p);
    for (int i = 0; i < 30; ++i) chunks.run(RunLimit::frames(1));
    CHECK(chunks.stateHash() == plain.stateHash());
    for (int mode = 0; mode < 5; ++mode) {
        Machine m = makeMachine(p);
        Debugger d(m);
        std::string error;
        if (mode == 0) d.setRingEnabled(true, 1000);
        if (mode == 1) d.setCallStackTracking(true);
        if (mode == 2) { Breakpoint bp; bp.kind = BpKind::Exec; bp.start = bp.end = 0x7000; d.add(bp, error); }
        if (mode == 3) { Breakpoint bp; bp.kind = BpKind::Write; bp.start = bp.end = 0xFF8000; d.add(bp, error); }
        if (mode == 4) { Breakpoint bp; bp.kind = BpKind::Exec; bp.cpu = CpuId::Z80; bp.start = bp.end = 0x7000; d.add(bp, error); }
        int stops = 0;
        for (int i = 0; i < 30; ++i) stops += d.run(RunLimit::frames(1)).reason != StopReason::LimitReached;
        INFO("mode " << mode << " stops " << stops << " frame " << m.frame() << " pc " << m.cpu().state().pc << " vs " << plain.cpu().state().pc << " z80 " << m.z80Cpu().state().pc << " vs " << plain.z80Cpu().state().pc << " cpuMclk " << m.cpuMclk() << " vs " << plain.cpuMclk() << " ram " << (int)m.bus().state().ram[0xF005] << " " << (int)plain.bus().state().ram[0xF005]);
        CHECK(m.stateHash() == plain.stateHash());
    }
}
