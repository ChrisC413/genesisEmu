#include "debug/debugger.hpp"

#include <algorithm>
#include <cinttypes>
#include <cstdio>
#include <fstream>

#include "analysis/disasm68k/disasm68k.hpp"
#include "analysis/disasmz80/disasmz80.hpp"
#include "debug/memory.hpp"
#include "instrument/trace_writer.hpp"

namespace gen::debug {

namespace {

constexpr std::size_t kMaxLog = 10000;
constexpr std::uint32_t k68kExecBits = 0x1000000 / 2;
constexpr std::uint32_t kZ80ExecBits = 0x10000;

bool testBit(const std::vector<std::uint64_t>& bits, std::uint32_t i) {
    return (bits[i >> 6] >> (i & 63) & 1) != 0;
}
void setBit(std::vector<std::uint64_t>& bits, std::uint32_t i) {
    bits[i >> 6] |= std::uint64_t{1} << (i & 63);
}

std::size_t cpuIndex(CpuId cpu) {
    return cpu == CpuId::Z80 ? 1 : 0;
}

// 68k work RAM is mirrored through $E00000-$FFFFFF; watchpoints use the canonical $FFxxxx address.
std::uint32_t canonical(CpuId cpu, std::uint32_t addr) {
    if (cpu == CpuId::Z80) {
        return addr & 0xFFFF;
    }
    addr &= 0xFFFFFF;
    return addr >= 0xE00000 ? (addr | 0xFF0000) : addr;
}

bool is68kCall(std::uint16_t op) {
    return (op & 0xFF00) == 0x6100 || (op & 0xFFC0) == 0x4E80 || (op & 0xFFF0) == 0x4E40;
}
bool is68kReturn(std::uint16_t op) {
    return op == 0x4E75 || op == 0x4E73 || op == 0x4E77;
}
bool isZ80Call(std::uint8_t op) {
    return op == 0xCD || (op & 0xC7) == 0xC4 || (op & 0xC7) == 0xC7;
}

const char* cpuName(CpuId cpu) {
    return cpu == CpuId::Z80 ? "Z80" : "68k";
}

std::string hex(std::uint32_t v, int digits) {
    char buf[16];
    std::snprintf(buf, sizeof buf, "$%0*X", digits, v);
    return buf;
}

}  // namespace

const char* toString(BpKind kind) {
    switch (kind) {
        case BpKind::Exec:
            return "exec";
        case BpKind::Read:
            return "read";
        case BpKind::Write:
            return "write";
        case BpKind::Access:
            return "read/write";
        case BpKind::VdpReg:
            return "VDP register";
        case BpKind::Vram:
            return "VRAM write";
        case BpKind::Cram:
            return "CRAM write";
        case BpKind::Vsram:
            return "VSRAM write";
        case BpKind::Dma:
            return "DMA";
        case BpKind::Interrupt:
            return "interrupt";
        case BpKind::Exception:
            return "exception";
        case BpKind::Frame:
            return "frame";
    }
    return "?";
}

std::string describeVector(int vector) {
    switch (vector) {
        case 2:
            return "bus error";
        case 3:
            return "address error";
        case 4:
            return "illegal instruction";
        case 5:
            return "divide by zero";
        case 6:
            return "CHK";
        case 7:
            return "TRAPV";
        case 8:
            return "privilege violation";
        case 9:
            return "trace";
        case 10:
            return "line A";
        case 11:
            return "line F";
        case 24:
            return "spurious interrupt";
        case 26:
            return "external interrupt (level 2)";
        case 28:
            return "H-int (level 4)";
        case 30:
            return "V-int (level 6)";
        default:
            break;
    }
    if (vector >= 25 && vector <= 31) {
        return "interrupt level " + std::to_string(vector - 24);
    }
    if (vector >= 32 && vector <= 47) {
        return "trap #" + std::to_string(vector - 32);
    }
    return "vector " + std::to_string(vector);
}

// Expression evaluation against the machine (side-effect-free reads).
class Debugger::Context final : public EvalContext {
public:
    Context(const Debugger& d, CpuId cpu, std::uint32_t addr, std::uint32_t value, std::uint64_t hits)
        : d_(d), cpu_(cpu), addr_(addr), value_(value), hits_(hits) {}

    std::int64_t reg(int id) const override {
        if (id >= kRegZ80) {
            const z80::Z80State& z = d_.m_.z80Cpu().state();
            const auto pair = [](std::uint8_t hi, std::uint8_t lo) { return (hi << 8) | lo; };
            switch (id - kRegZ80) {
                case 0:
                    return z.a;
                case 1:
                    return z.f;
                case 2:
                    return z.b;
                case 3:
                    return z.c;
                case 4:
                    return z.d;
                case 5:
                    return z.e;
                case 6:
                    return z.h;
                case 7:
                    return z.l;
                case 8:
                    return z.i;
                case 9:
                    return z.r;
                case 10:
                    return pair(z.a, z.f);
                case 11:
                    return pair(z.b, z.c);
                case 12:
                    return pair(z.d, z.e);
                case 13:
                    return pair(z.h, z.l);
                case 14:
                    return z.ix;
                case 15:
                    return z.iy;
                case 16:
                    return z.sp;
                case 17:
                    return z.pc;
                case 18:
                    return z.af2;
                case 19:
                    return z.bc2;
                case 20:
                    return z.de2;
                case 21:
                    return z.hl2;
                default:
                    return 0;
            }
        }
        const m68k::State& s = d_.m_.cpu().state();
        if (id < 8) {
            return s.d[static_cast<std::size_t>(id)];
        }
        if (id < 16) {
            return s.a[static_cast<std::size_t>(id - 8)];
        }
        switch (id) {
            case kRegPc:
                return s.pc;
            case kRegSr:
                return s.sr;
            case kRegCcr:
                return s.sr & 0xFF;
            case kRegUsp:
                return s.userSp();
            case kRegSsp:
                return s.supervisorSp();
            default:
                return 0;
        }
    }
    std::uint32_t read(std::uint32_t addr, int size) const override {
        return peekValue(d_.m_, cpu_, addr, size);
    }
    std::int64_t variable(int id) const override {
        switch (id) {
            case kVarFrame:
                return static_cast<std::int64_t>(d_.m_.frame());
            case kVarLine:
                return d_.m_.line();
            case kVarValue:
                return value_;
            case kVarAddr:
                return addr_;
            case kVarHits:
                return static_cast<std::int64_t>(hits_);
            default:
                return 0;
        }
    }

private:
    const Debugger& d_;
    CpuId cpu_;
    std::uint32_t addr_;
    std::uint32_t value_;
    std::uint64_t hits_;
};

Debugger::Debugger(Machine& machine) : m_(machine) {}

Debugger::~Debugger() {
    endTrace();
    if (m_.hooks().ctx == this) {
        m_.setHooks(Hooks{});
    }
}

// ---- Hooks -------------------------------------------------------------------------------------------

bool Debugger::hooksInstalled() const {
    const Hooks& h = m_.hooks();
    return h.onInstr != nullptr || h.onAccess != nullptr || h.onDma != nullptr || h.onIrq != nullptr ||
           h.onException != nullptr || h.onVdpWrite != nullptr || h.onFrameEnd != nullptr;
}

void Debugger::refreshHooks() {
    const bool anyBp = std::any_of(bps_.begin(), bps_.end(), [](const Breakpoint& b) { return b.enabled; });
    Hooks h;
    h.ctx = this;
    if (anyBp || stepPending() || z80Frozen_ || trace_ || ringEnabled_ || callTracking_) {
        h.onInstr = &Debugger::onInstr;
    }
    if (anyWatch_ || trace_) {
        h.onAccess = &Debugger::onAccess;
    }
    if (anyDma_) {
        h.onDma = &Debugger::onDma;
    }
    if (anyIrq_ || trace_ || ringEnabled_) {
        h.onIrq = &Debugger::onIrq;
    }
    if (anyException_ || callTracking_) {
        h.onException = &Debugger::onException;
    }
    if (anyVdpWrite_) {
        h.onVdpWrite = &Debugger::onVdpWrite;
    }
    if (anyFrame_) {
        h.onFrameEnd = &Debugger::onFrameEnd;
    }
    if (h.onInstr == nullptr && h.onAccess == nullptr && h.onDma == nullptr && h.onIrq == nullptr &&
        h.onException == nullptr && h.onVdpWrite == nullptr && h.onFrameEnd == nullptr) {
        h = Hooks{};  // nothing active: the core runs exactly as without a debugger
    }
    m_.setHooks(h);
}

Action Debugger::onInstr(void* ctx, CpuId cpu, std::uint32_t pc, std::uint16_t opcode) {
    return static_cast<Debugger*>(ctx)->instr(cpu, pc, opcode);
}

Action Debugger::instr(CpuId cpu, std::uint32_t pc, std::uint16_t opcode) {
    Skip& skip = skip_[cpuIndex(cpu)];
    bool skipped = false;
    if (skip.armed) {
        skip.armed = false;
        skipped = skip.pc == pc;
    }
    if (cpu == CpuId::Z80) {
        if (z80Frozen_) {
            return Action::Break;  // a Z80 stop is pending: start nothing more until resumed
        }
    }
    if (!skipped) {
        const bool indexed = cpu == CpuId::Z80 ? anyExecZ80_ && testBit(execBitsZ80_, pc & 0xFFFF)
                                               : anyExec68k_ && testBit(execBits68k_, (pc & 0xFFFFFF) >> 1);
        if (indexed) {
            for (Breakpoint& bp : bps_) {
                if (bp.enabled && bp.kind == BpKind::Exec && bp.cpu == cpu && pc >= bp.start && pc <= bp.end &&
                    hit(bp, cpu, pc, 0)) {
                    stopAt(StopInfo::Kind::Breakpoint, &bp, cpu, pc, pc, 0);
                    if (cpu == CpuId::Z80) {
                        z80Frozen_ = true;
                    }
                    return Action::Break;
                }
            }
        }
        if (step_.mode != StepMode::None && step_.cpu == cpu && stepDone(cpu, pc)) {
            stopAt(StopInfo::Kind::Step, nullptr, cpu, pc, 0, 0);
            step_ = StepState{};
            if (cpu == CpuId::Z80) {
                z80Frozen_ = true;
            }
            return Action::Break;
        }
    }
    if (step_.mode == StepMode::Out && step_.cpu == cpu) {
        if (cpu == CpuId::Z80) {
            const auto op = static_cast<std::uint8_t>(opcode);
            const std::uint32_t next = peekValue(m_, CpuId::Z80, pc + 1, 1);
            step_.lastWasReturn =
                op == 0xC9 || (op & 0xC7) == 0xC0 || (op == 0xED && (next == 0x4D || next == 0x45));
        } else {
            step_.lastWasReturn = is68kReturn(opcode);
        }
    }
    // The instruction will execute: observers record it.
    if (cpu == CpuId::Z80) {
        if (ringEnabled_) {
            recordRing(cpu, pc, 0);
        }
        return Action::Continue;
    }
    if (callTracking_) {
        trackCalls(pc);
        if (is68kCall(opcode) && (opcode & 0xFFF0) != 0x4E40) {
            pendingCall_ = {true, pc, m_.cpu().state().a[7]};
        }
    }
    if (trace_) {
        trace_->instr(pc);
    }
    if (ringEnabled_) {
        recordRing(cpu, pc, 0);
    }
    return Action::Continue;
}

bool Debugger::stepDone(CpuId cpu, std::uint32_t pc) {
    switch (step_.mode) {
        case StepMode::Over:
            return pc == step_.target && sp(cpu) >= step_.sp;
        case StepMode::Out:
            return step_.lastWasReturn && sp(cpu) > step_.sp;
        case StepMode::RunTo:
            return pc == step_.target;
        case StepMode::None:
            break;
    }
    return false;
}

void Debugger::onAccess(void* ctx, CpuId cpu, AccessKind kind, std::uint32_t addr, std::uint8_t size,
                        std::uint32_t value, std::uint32_t pc) {
    auto& d = *static_cast<Debugger*>(ctx);
    if (kind == AccessKind::Fetch) {
        return;
    }
    if (d.trace_ && cpu == CpuId::M68k && kind == AccessKind::Write) {
        d.trace_->write(addr, size, value);
    }
    if (!d.anyWatch_) {
        return;
    }
    const std::uint32_t a = canonical(cpu, addr);
    const std::uint32_t last = a + size - 1;
    const auto& pages = cpu == CpuId::Z80 ? d.watchPagesZ80_ : d.watchPages68k_;
    if (pages.empty() || (!testBit(pages, a >> 8) && !testBit(pages, (last >> 8) & (cpu == CpuId::Z80 ? 0xFF : 0xFFFF)))) {
        return;
    }
    const auto sizeBit = static_cast<std::uint8_t>(size == 1 ? 1 : size == 2 ? 2 : 4);
    for (Breakpoint& bp : d.bps_) {
        if (!bp.enabled || bp.cpu != cpu || (bp.sizes & sizeBit) == 0) {
            continue;
        }
        const bool kindOk = bp.kind == BpKind::Access || (bp.kind == BpKind::Read && kind == AccessKind::Read) ||
                            (bp.kind == BpKind::Write && kind == AccessKind::Write);
        if (!kindOk || last < bp.start || a > bp.end) {
            continue;
        }
        if (d.hit(bp, cpu, a, value)) {
            d.requestStop(StopInfo::Kind::Watchpoint, &bp, cpu, pc, a, value);
        }
    }
}

void Debugger::onDma(void* ctx, const DmaEvent& event) {
    auto& d = *static_cast<Debugger*>(ctx);
    for (Breakpoint& bp : d.bps_) {
        if (!bp.enabled || bp.kind != BpKind::Dma) {
            continue;
        }
        if (bp.dmaTarget >= 0 && bp.dmaTarget != static_cast<int>(event.target)) {
            continue;
        }
        if (event.dest > bp.end || event.dest < bp.start) {
            continue;
        }
        if (d.hit(bp, CpuId::M68k, event.dest, event.source)) {
            d.requestStop(StopInfo::Kind::Watchpoint, &bp, CpuId::M68k, d.m_.cpu().currentPc(), event.dest,
                          event.source);
        }
    }
}

void Debugger::onIrq(void* ctx, CpuId cpu, int level) {
    auto& d = *static_cast<Debugger*>(ctx);
    if (cpu == CpuId::M68k) {
        if (d.trace_) {
            d.trace_->irq(level);
        }
        if (d.ringEnabled_) {
            d.recordRing(cpu, d.m_.cpu().currentPc(), level);
        }
    }
    if (!d.anyIrq_) {
        return;
    }
    for (Breakpoint& bp : d.bps_) {
        if (bp.enabled && bp.kind == BpKind::Interrupt && bp.cpu == cpu &&
            static_cast<std::uint32_t>(level) >= bp.start && static_cast<std::uint32_t>(level) <= bp.end &&
            d.hit(bp, cpu, static_cast<std::uint32_t>(level), 0)) {
            d.requestStop(StopInfo::Kind::Watchpoint, &bp, cpu, d.pcOf(cpu), static_cast<std::uint32_t>(level),
                          0);
        }
    }
}

void Debugger::onException(void* ctx, CpuId cpu, int vector, std::uint32_t pc) {
    auto& d = *static_cast<Debugger*>(ctx);
    if (d.callTracking_ && cpu == CpuId::M68k) {
        d.pendingException_ = {true, vector, pc};
    }
    if (!d.anyException_) {
        return;
    }
    for (Breakpoint& bp : d.bps_) {
        if (bp.enabled && bp.kind == BpKind::Exception && static_cast<std::uint32_t>(vector) >= bp.start &&
            static_cast<std::uint32_t>(vector) <= bp.end &&
            d.hit(bp, cpu, static_cast<std::uint32_t>(vector), pc)) {
            d.requestStop(StopInfo::Kind::Watchpoint, &bp, cpu, pc, static_cast<std::uint32_t>(vector), pc);
        }
    }
}

void Debugger::onVdpWrite(void* ctx, VdpWriteKind kind, std::uint32_t addr, std::uint16_t value) {
    auto& d = *static_cast<Debugger*>(ctx);
    BpKind want = BpKind::VdpReg;
    switch (kind) {
        case VdpWriteKind::Register:
            want = BpKind::VdpReg;
            break;
        case VdpWriteKind::Vram:
            want = BpKind::Vram;
            break;
        case VdpWriteKind::Cram:
            want = BpKind::Cram;
            break;
        case VdpWriteKind::Vsram:
            want = BpKind::Vsram;
            break;
    }
    for (Breakpoint& bp : d.bps_) {
        if (bp.enabled && bp.kind == want && addr + (kind == VdpWriteKind::Register ? 0 : 1) >= bp.start &&
            addr <= bp.end && d.hit(bp, CpuId::M68k, addr, value)) {
            d.requestStop(StopInfo::Kind::Watchpoint, &bp, CpuId::M68k, d.m_.cpu().currentPc(), addr, value);
        }
    }
}

void Debugger::onFrameEnd(void* ctx, std::uint64_t frame) {
    auto& d = *static_cast<Debugger*>(ctx);
    const std::uint64_t next = frame + 1;  // the frame that begins now
    for (Breakpoint& bp : d.bps_) {
        if (bp.enabled && bp.kind == BpKind::Frame && next >= bp.start && next <= bp.end &&
            d.hit(bp, CpuId::M68k, static_cast<std::uint32_t>(next), 0)) {
            d.requestStop(StopInfo::Kind::Watchpoint, &bp, CpuId::M68k, d.m_.cpu().state().pc,
                          static_cast<std::uint32_t>(next), 0);
        }
    }
}

bool Debugger::hit(Breakpoint& bp, CpuId cpu, std::uint32_t addr, std::uint32_t value) {
    const Context ctx(*this, cpu, addr, value, bp.hits + 1);
    if (bp.compiledCondition && bp.compiledCondition->eval(ctx) == 0) {
        return false;
    }
    ++bp.hits;
    if (bp.hits <= bp.ignoreHits) {
        return false;
    }
    if (bp.log && bp.compiledMessage) {
        log_.push_back(bp.compiledMessage->render(ctx));
        if (log_.size() > kMaxLog) {
            log_.pop_front();
        }
    }
    if (bp.traceFrames != 0) {
        startTraceOnHit(bp.traceFrames);
    }
    return !bp.log || bp.stop;
}

void Debugger::stopAt(StopInfo::Kind kind, const Breakpoint* bp, CpuId cpu, std::uint32_t pc,
                      std::uint32_t addr, std::uint32_t value) {
    stopping_ = true;
    stop_ = StopInfo{};
    stop_.kind = kind;
    stop_.cpu = cpu;
    stop_.pc = pc;
    stop_.address = addr;
    stop_.value = value;
    const int digits = cpu == CpuId::Z80 ? 4 : 6;
    if (bp != nullptr) {
        stop_.breakpoint = bp->id;
        stop_.text = std::string(toString(bp->kind)) + " breakpoint #" + std::to_string(bp->id);
        switch (bp->kind) {
            case BpKind::Exec:
                stop_.text += " at " + hex(pc, digits);
                break;
            case BpKind::Read:
            case BpKind::Write:
            case BpKind::Access:
                stop_.text += ": " + hex(addr, digits) + " = " + hex(value, 2) + " by " + hex(pc, digits);
                break;
            case BpKind::VdpReg:
                stop_.text += ": reg " + std::to_string(addr) + " = " + hex(value, 2) + " by " + hex(pc, 6);
                break;
            case BpKind::Vram:
            case BpKind::Cram:
            case BpKind::Vsram:
                stop_.text += ": " + hex(addr, 4) + " = " + hex(value, 4) + " by " + hex(pc, 6);
                break;
            case BpKind::Dma:
                stop_.text += ": to " + hex(addr, 4) + " from " + hex(value, 6) + " by " + hex(pc, 6);
                break;
            case BpKind::Interrupt:
                stop_.text += ": level " + std::to_string(addr);
                break;
            case BpKind::Exception:
                stop_.text += ": " + describeVector(static_cast<int>(addr)) + " from " + hex(value, 6);
                break;
            case BpKind::Frame:
                stop_.text += ": frame " + std::to_string(addr);
                break;
        }
    } else if (kind == StopInfo::Kind::Step) {
        stop_.text = std::string("step (") + cpuName(cpu) + ") at " + hex(pc, digits);
    }
}

void Debugger::requestStop(StopInfo::Kind kind, const Breakpoint* bp, CpuId cpu, std::uint32_t pc,
                           std::uint32_t addr, std::uint32_t value) {
    if (stopping_) {
        return;  // the first event of an instruction is reported
    }
    stopAt(kind, bp, cpu, pc, addr, value);
    m_.requestStop(StopReason::Watchpoint, cpu);
    if (cpu == CpuId::Z80) {
        z80Frozen_ = true;
    }
}

// ---- Run control -------------------------------------------------------------------------------------

std::uint32_t Debugger::sp(CpuId cpu) const {
    return cpu == CpuId::Z80 ? m_.z80Cpu().state().sp : m_.cpu().state().a[7];
}

std::uint32_t Debugger::pcOf(CpuId cpu) const {
    return cpu == CpuId::Z80 ? m_.z80Cpu().state().pc : m_.cpu().state().pc;
}

std::uint32_t Debugger::read(CpuId cpu, std::uint32_t addr, int size) const {
    return peekValue(m_, cpu, addr, size);
}

void Debugger::armSkips() {
    skip_[0] = {m_.cpu().state().pc, true};
    skip_[1] = {m_.z80Cpu().state().pc, true};
    z80Frozen_ = false;
    stopping_ = false;
}

void Debugger::finishRun(const RunResult& r) {
    if (r.reason == StopReason::Exception) {
        stop_ = StopInfo{};
        stop_.kind = StopInfo::Kind::Fault;
        stop_.pc = m_.cpu().state().pc;
        stop_.text = "fault: " + describeVector(r.vector);
    } else if ((r.reason == StopReason::Breakpoint || r.reason == StopReason::Watchpoint) && !stopping_) {
        stop_ = StopInfo{};
        stop_.kind = StopInfo::Kind::Breakpoint;
        stop_.cpu = r.cpu;
        stop_.pc = pcOf(r.cpu);
        stop_.text = "break";
    }
    if (r.reason != StopReason::LimitReached && stop_.breakpoint >= 0) {
        if (const Breakpoint* bp = find(stop_.breakpoint); bp != nullptr && bp->temporary) {
            remove(stop_.breakpoint);
        }
    }
    stopping_ = false;
    z80Frozen_ = false;
    if (r.reason != StopReason::LimitReached) {
        step_ = StepState{};  // a breakpoint or watchpoint ends a step over / out / run-to
    }
    if (trace_ && trace_->done()) {
        endTrace();
    }
    refreshHooks();
}

RunResult Debugger::run(RunLimit limit) {
    armSkips();
    refreshHooks();
    const RunResult r = m_.run(limit);
    finishRun(r);
    return r;
}

RunResult Debugger::stepInto(CpuId cpu) {
    RunResult r;
    if (cpu == CpuId::Z80) {
        if (!m_.z80().running()) {
            stop_ = StopInfo{};
            stop_.kind = StopInfo::Kind::Step;
            stop_.cpu = CpuId::Z80;
            stop_.pc = pcOf(CpuId::Z80);
            stop_.text = "the Z80 is held (BUSREQ / RESET): nothing to step";
            return r;
        }
        // The Z80 runs behind the 68k and catches up when the 68k touches it or a line ends. Running one
        // Z80 instruction while it is behind gives exactly what free running would; when it is ahead, the
        // 68k first runs (with the Z80 held at its instruction) until it is behind again.
        armSkips();
        z80Frozen_ = true;
        refreshHooks();
        for (int guard = 0; guard < 1000000 && m_.z80Mclk() >= m_.cpuMclk(); ++guard) {
            const RunResult s = m_.step(CpuId::M68k);
            r.instructions += s.instructions;
            if (s.reason != StopReason::LimitReached && !(s.reason == StopReason::Breakpoint && s.cpu == CpuId::Z80)) {
                r.reason = s.reason;
                r.cpu = s.cpu;
                finishRun(r);
                return r;
            }
        }
        z80Frozen_ = false;
        skip_[1] = {m_.z80Cpu().state().pc, true};
        const RunResult s = m_.step(CpuId::Z80);
        if (s.reason == StopReason::LimitReached) {
            stopAt(StopInfo::Kind::Step, nullptr, CpuId::Z80, m_.z80Cpu().state().pc, 0, 0);
        }
        r.reason = s.reason;
        r.cpu = s.cpu;
        finishRun(r);
        return r;
    }
    armSkips();
    refreshHooks();
    r = m_.step(CpuId::M68k);
    if (r.reason == StopReason::LimitReached) {
        stopAt(StopInfo::Kind::Step, nullptr, CpuId::M68k, m_.cpu().state().pc, 0, 0);
    }
    finishRun(r);
    return r;
}

bool Debugger::beginStepOver(CpuId cpu) {
    const std::uint32_t pc = pcOf(cpu);
    bool call = false;
    if (cpu == CpuId::Z80) {
        call = isZ80Call(static_cast<std::uint8_t>(read(cpu, pc, 1)));
    } else {
        const m68k::State& s = m_.cpu().state();
        call = !s.stopped && is68kCall(s.ir);
    }
    if (!call) {
        stepInto(cpu);
        return false;
    }
    int length = 2;
    disassemble(cpu, pc, &length);
    step_ = StepState{StepMode::Over, cpu, (pc + static_cast<std::uint32_t>(length)) & 0xFFFFFF, sp(cpu), false};
    return true;
}

void Debugger::beginStepOut(CpuId cpu) {
    step_ = StepState{StepMode::Out, cpu, 0, sp(cpu), false};
}

void Debugger::beginRunTo(CpuId cpu, std::uint32_t addr) {
    step_ = StepState{StepMode::RunTo, cpu, addr, 0, false};
}

void Debugger::cancelStep() {
    step_ = StepState{};
    refreshHooks();
}

void Debugger::notePause() {
    stop_ = StopInfo{};
    stop_.kind = StopInfo::Kind::Pause;
    stop_.pc = m_.cpu().state().pc;
    stop_.text = "paused";
}

namespace {
RunResult runPending(Debugger& d, std::uint64_t maxFrames) {
    RunResult total;
    for (std::uint64_t f = 0; f < maxFrames && d.stepPending(); ++f) {
        const RunResult r = d.run(RunLimit::frames(1));
        total.instructions += r.instructions;
        if (r.reason != StopReason::LimitReached) {
            total.reason = r.reason;
            total.cpu = r.cpu;
            total.vector = r.vector;
            return total;
        }
    }
    return total;
}
}  // namespace

RunResult Debugger::stepOver(CpuId cpu, std::uint64_t maxFrames) {
    if (!beginStepOver(cpu)) {
        RunResult r;
        r.instructions = 1;
        return r;
    }
    const RunResult r = runPending(*this, maxFrames);
    if (stepPending()) {
        cancelStep();
        stop_.kind = StopInfo::Kind::Limit;
        stop_.text = "step over did not return within the frame limit";
    }
    return r;
}

RunResult Debugger::stepOut(CpuId cpu, std::uint64_t maxFrames) {
    beginStepOut(cpu);
    const RunResult r = runPending(*this, maxFrames);
    if (stepPending()) {
        cancelStep();
        stop_.kind = StopInfo::Kind::Limit;
        stop_.text = "step out did not return within the frame limit";
    }
    return r;
}

RunResult Debugger::runTo(CpuId cpu, std::uint32_t addr, std::uint64_t maxFrames) {
    beginRunTo(cpu, addr);
    const RunResult r = runPending(*this, maxFrames);
    if (stepPending()) {
        cancelStep();
        stop_.kind = StopInfo::Kind::Limit;
        stop_.text = "run to cursor: address not reached within the frame limit";
    }
    return r;
}

// ---- Breakpoints -------------------------------------------------------------------------------------

namespace {
bool compile(Breakpoint& bp, std::string& error) {
    bp.compiledCondition.reset();
    bp.compiledMessage.reset();
    if (!bp.condition.empty()) {
        bp.compiledCondition = Expr::compile(bp.condition, bp.cpu, error);
        if (!bp.compiledCondition) {
            error = "condition: " + error;
            return false;
        }
    }
    if (bp.log) {
        bp.compiledMessage = Format::compile(bp.message, bp.cpu, error);
        if (!bp.compiledMessage) {
            error = "message: " + error;
            return false;
        }
    }
    if (bp.end < bp.start) {
        bp.end = bp.start;
    }
    return true;
}
}  // namespace

int Debugger::add(Breakpoint bp, std::string& error) {
    if (!compile(bp, error)) {
        return -1;
    }
    if (bp.kind == BpKind::Read || bp.kind == BpKind::Write || bp.kind == BpKind::Access) {
        bp.start = canonical(bp.cpu, bp.start);
        bp.end = canonical(bp.cpu, bp.end);
    }
    bp.id = nextId_++;
    bp.hits = 0;
    bps_.push_back(std::move(bp));
    rebuild();
    return bps_.back().id;
}

bool Debugger::update(const Breakpoint& bp, std::string& error) {
    Breakpoint* existing = find(bp.id);
    if (existing == nullptr) {
        error = "no breakpoint #" + std::to_string(bp.id);
        return false;
    }
    Breakpoint copy = bp;
    if (!compile(copy, error)) {
        return false;
    }
    *existing = std::move(copy);
    rebuild();
    return true;
}

bool Debugger::remove(int id) {
    const auto it = std::find_if(bps_.begin(), bps_.end(), [&](const Breakpoint& b) { return b.id == id; });
    if (it == bps_.end()) {
        return false;
    }
    bps_.erase(it);
    rebuild();
    return true;
}

void Debugger::clearBreakpoints() {
    bps_.clear();
    rebuild();
}

Breakpoint* Debugger::find(int id) {
    const auto it = std::find_if(bps_.begin(), bps_.end(), [&](const Breakpoint& b) { return b.id == id; });
    return it == bps_.end() ? nullptr : &*it;
}

bool Debugger::hasExec(CpuId cpu, std::uint32_t addr) const {
    return std::any_of(bps_.begin(), bps_.end(), [&](const Breakpoint& b) {
        return b.kind == BpKind::Exec && b.cpu == cpu && b.start == addr && b.end == addr;
    });
}

bool Debugger::toggleExec(CpuId cpu, std::uint32_t addr) {
    const auto it = std::find_if(bps_.begin(), bps_.end(), [&](const Breakpoint& b) {
        return b.kind == BpKind::Exec && b.cpu == cpu && b.start == addr && b.end == addr;
    });
    if (it != bps_.end()) {
        bps_.erase(it);
        rebuild();
        return false;
    }
    Breakpoint bp;
    bp.kind = BpKind::Exec;
    bp.cpu = cpu;
    bp.start = addr;
    bp.end = addr;
    std::string ignored;
    add(std::move(bp), ignored);
    return true;
}

void Debugger::setEnabled(int id, bool enabled) {
    if (Breakpoint* bp = find(id)) {
        bp->enabled = enabled;
        rebuild();
    }
}

void Debugger::rebuild() {
    anyExec68k_ = anyExecZ80_ = anyWatch_ = anyVdpWrite_ = anyDma_ = anyIrq_ = anyException_ = anyFrame_ =
        false;
    std::fill(execBits68k_.begin(), execBits68k_.end(), 0);
    std::fill(execBitsZ80_.begin(), execBitsZ80_.end(), 0);
    std::fill(watchPages68k_.begin(), watchPages68k_.end(), 0);
    std::fill(watchPagesZ80_.begin(), watchPagesZ80_.end(), 0);
    for (const Breakpoint& bp : bps_) {
        if (!bp.enabled) {
            continue;
        }
        switch (bp.kind) {
            case BpKind::Exec:
                if (bp.cpu == CpuId::Z80) {
                    execBitsZ80_.resize(kZ80ExecBits / 64);
                    for (std::uint32_t a = bp.start; a <= std::min<std::uint32_t>(bp.end, 0xFFFF); ++a) {
                        setBit(execBitsZ80_, a);
                    }
                    anyExecZ80_ = true;
                } else {
                    execBits68k_.resize(k68kExecBits / 64);
                    for (std::uint32_t a = bp.start & 0xFFFFFE; a <= std::min<std::uint32_t>(bp.end, 0xFFFFFF);
                         a += 2) {
                        setBit(execBits68k_, a >> 1);
                    }
                    anyExec68k_ = true;
                }
                break;
            case BpKind::Read:
            case BpKind::Write:
            case BpKind::Access: {
                auto& pages = bp.cpu == CpuId::Z80 ? watchPagesZ80_ : watchPages68k_;
                const std::uint32_t maxPage = bp.cpu == CpuId::Z80 ? 0xFF : 0xFFFF;
                pages.resize((maxPage + 1) / 64);
                for (std::uint32_t p = bp.start >> 8; p <= std::min(bp.end >> 8, maxPage); ++p) {
                    setBit(pages, p);
                }
                anyWatch_ = true;
                break;
            }
            case BpKind::VdpReg:
            case BpKind::Vram:
            case BpKind::Cram:
            case BpKind::Vsram:
                anyVdpWrite_ = true;
                break;
            case BpKind::Dma:
                anyDma_ = true;
                break;
            case BpKind::Interrupt:
                anyIrq_ = true;
                break;
            case BpKind::Exception:
                anyException_ = true;
                break;
            case BpKind::Frame:
                anyFrame_ = true;
                break;
        }
    }
    refreshHooks();
}

// ---- Trace and ring buffer ---------------------------------------------------------------------------

std::string Debugger::disassemble(CpuId cpu, std::uint32_t pc, int* length) const {
    std::array<std::uint8_t, 10> bytes{};
    std::size_t n = 0;
    for (; n < bytes.size(); ++n) {
        const auto b = peek(m_, cpu, pc + static_cast<std::uint32_t>(n));
        if (!b) {
            break;
        }
        bytes[n] = *b;
    }
    const std::span<const std::uint8_t> span(bytes.data(), n);
    analysis::disasm68k::Instruction in =
        cpu == CpuId::Z80 ? analysis::disasmz80::disassemble(span, static_cast<std::uint16_t>(pc))
                          : analysis::disasm68k::disassemble(span, pc);
    if (length != nullptr) {
        *length = in.length;
    }
    std::string text = in.text();
    std::replace(text.begin(), text.end(), '\t', ' ');
    return text;
}

bool Debugger::beginTrace(const std::string& path, const TraceOptions& options, std::string& error) {
    endTrace();
    auto file = std::make_unique<std::ofstream>(path, std::ios::binary);
    if (!*file) {
        error = "cannot write '" + path + "'";
        return false;
    }
    instrument::TraceConfig config;
    config.startFrame = m_.frame();
    config.endFrame = options.frames != 0 ? m_.frame() + options.frames : UINT64_MAX;
    config.pcRange = options.pcRange;
    config.firstExecutionOnly = options.firstExecutionOnly;
    config.maxRecords = options.maxRecords;
    traceFile_ = std::move(file);
    trace_ = std::make_unique<instrument::TraceWriter>(
        m_, traceFile_.get(), config, [](std::span<const std::uint8_t> bytes, std::uint32_t addr, int& length) {
            const auto in = analysis::disasm68k::disassemble(bytes, addr);
            length = in.length;
            return in.text();
        });
    tracePath_ = path;
    refreshHooks();
    return true;
}

void Debugger::endTrace() {
    if (!trace_) {
        return;
    }
    trace_->finish();
    trace_.reset();
    traceFile_.reset();
    refreshHooks();
}

std::uint64_t Debugger::traceRecords() const {
    return trace_ ? trace_->records() : 0;
}

void Debugger::startTraceOnHit(std::uint32_t frames) {
    if (trace_ || traceOnHitPath_.empty()) {
        return;
    }
    TraceOptions o;
    o.frames = frames;
    std::string ignored;
    beginTrace(traceOnHitPath_, o, ignored);
}

void Debugger::setRingEnabled(bool enabled, std::size_t capacity) {
    ringEnabled_ = enabled;
    if (enabled) {
        capacity = std::max<std::size_t>(capacity, 16);
        if (ringBuf_.size() != capacity) {
            ringBuf_.assign(capacity, RingEntry{});
            ringNext_ = 0;
            ringFull_ = false;
        }
    } else {
        ringBuf_.clear();
        ringBuf_.shrink_to_fit();
        ringNext_ = 0;
        ringFull_ = false;
    }
    refreshHooks();
}

void Debugger::recordRing(CpuId cpu, std::uint32_t pc, int irq) {
    RingEntry& e = ringBuf_[ringNext_];
    e.mclk = cpu == CpuId::Z80 ? m_.z80Mclk() : m_.cpuMclk();
    e.frame = static_cast<std::uint32_t>(m_.frame());
    e.line = static_cast<std::uint16_t>(m_.line());
    e.pc = pc;
    e.cpu = cpu;
    e.irq = static_cast<std::int8_t>(irq);
    if (++ringNext_ == ringBuf_.size()) {
        ringNext_ = 0;
        ringFull_ = true;
    }
}

std::vector<RingEntry> Debugger::ring() const {
    std::vector<RingEntry> out;
    if (ringBuf_.empty()) {
        return out;
    }
    if (ringFull_) {
        out.insert(out.end(), ringBuf_.begin() + static_cast<std::ptrdiff_t>(ringNext_), ringBuf_.end());
    }
    out.insert(out.end(), ringBuf_.begin(), ringBuf_.begin() + static_cast<std::ptrdiff_t>(ringNext_));
    return out;
}

void Debugger::dumpRing(std::ostream& out) const {
    out << "# genesisEmu ring buffer: frame line mclk cpu pc disassembly (oldest first)\n";
    char buf[96];
    for (const RingEntry& e : ring()) {
        std::snprintf(buf, sizeof buf, "%u %3u %10" PRIu64 " %s %06X  ", e.frame, e.line, e.mclk,
                      e.cpu == CpuId::Z80 ? "Z80 " : "M68K", e.pc);
        out << buf;
        if (e.irq > 0) {
            out << "interrupt level " << static_cast<int>(e.irq) << '\n';
        } else {
            out << disassemble(e.cpu, e.pc) << '\n';
        }
    }
}

// ---- Call stack --------------------------------------------------------------------------------------

void Debugger::setCallStackTracking(bool enabled) {
    callTracking_ = enabled;
    calls_.clear();
    pendingCall_ = {};
    pendingException_ = {};
    if (enabled) {
        // Seed from a stack walk so frames entered before tracking began are shown too.
        calls_ = walkStack();
    }
    refreshHooks();
}

void Debugger::trackCalls(std::uint32_t pc) {
    applyPending(calls_, pendingCall_, pendingException_, pc);
}

void Debugger::applyPending(std::vector<CallFrame>& frames, PendingCall& call,
                            PendingException& exception, std::uint32_t pc) const {
    const std::uint32_t spNow = m_.cpu().state().a[7];
    // Frames whose return address / exception frame is above the stack pointer have returned (RTS, RTE,
    // or code that dropped them, e.g. addq.l #4,sp).
    while (!frames.empty() && frames.back().sp < spNow) {
        frames.pop_back();
    }
    if (call.valid) {
        call.valid = false;
        const std::uint32_t slot = (call.sp - 4) & 0xFFFFFF;
        int length = 2;
        disassemble(CpuId::M68k, call.pc, &length);
        const std::uint32_t ret = call.pc + static_cast<std::uint32_t>(length);
        if (spNow <= slot && read(CpuId::M68k, slot, 4) == ret) {
            CallFrame f;
            f.kind = CallFrame::Kind::Call;
            f.callPc = call.pc;
            f.entry = exception.valid ? exception.pc : pc;
            f.returnPc = ret;
            f.sp = call.sp - 4;
            frames.push_back(f);
        }
    }
    if (exception.valid) {
        exception.valid = false;
        CallFrame f;
        const int v = exception.vector;
        f.kind = v >= 24 && v <= 31 ? CallFrame::Kind::Interrupt : CallFrame::Kind::Exception;
        f.vector = v;
        f.callPc = exception.pc;
        f.entry = pc;
        f.returnPc = read(CpuId::M68k, spNow + 2, 4) & 0xFFFFFF;
        f.sp = spNow;
        frames.push_back(f);
    }
}

std::vector<CallFrame> Debugger::callStack() const {
    if (!callTracking_) {
        return walkStack();
    }
    // Paused before the first instruction of a callee / handler: its frame is still pending.
    std::vector<CallFrame> frames = calls_;
    PendingCall call = pendingCall_;
    PendingException exception = pendingException_;
    applyPending(frames, call, exception, m_.cpu().state().pc);
    return frames;
}

std::vector<CallFrame> Debugger::walkStack(int maxFrames) const {
    // Heuristic: every long on the stack that points just past a JSR / BSR in ROM is a return address.
    std::vector<CallFrame> found;
    const std::uint32_t top = m_.cpu().state().a[7] & 0xFFFFFF;
    if (top < 0xE00000) {
        return found;
    }
    const std::uint32_t romSize = static_cast<std::uint32_t>(m_.rom().size());
    for (std::uint32_t at = top; at + 4 <= 0x1000000 && at < top + 0x400 &&
                                 static_cast<int>(found.size()) < maxFrames;
         at += 2) {
        const std::uint32_t v = read(CpuId::M68k, at, 4);
        if ((v & 1) != 0 || v < 0x200 || v >= romSize || v > 0xFFFFFF) {
            continue;
        }
        std::optional<std::uint32_t> callPc;
        std::optional<std::uint32_t> entry;
        const auto word = [&](std::uint32_t a) { return static_cast<std::uint16_t>(read(CpuId::M68k, a, 2)); };
        const std::uint16_t w2 = word(v - 2);
        const std::uint16_t w4 = word(v - 4);
        const std::uint16_t w6 = word(v - 6);
        if ((w2 & 0xFF00) == 0x6100 && (w2 & 0xFF) != 0) {
            callPc = v - 2;
            entry = v + static_cast<std::uint32_t>(static_cast<std::int8_t>(w2 & 0xFF));
        } else if ((w2 & 0xFFF8) == 0x4E90) {
            callPc = v - 2;  // jsr (an)
        } else if (w4 == 0x6100) {
            callPc = v - 4;
            entry = v - 2 + static_cast<std::uint32_t>(static_cast<std::int16_t>(word(v - 2)));
        } else if ((w4 & 0xFFF8) == 0x4EA8 || (w4 & 0xFFF8) == 0x4EB0 || w4 == 0x4EBB) {
            callPc = v - 4;
        } else if (w4 == 0x4EB8) {
            callPc = v - 4;
            entry = static_cast<std::uint32_t>(static_cast<std::int16_t>(word(v - 2))) & 0xFFFFFF;
        } else if (w4 == 0x4EBA) {
            callPc = v - 4;
            entry = v - 2 + static_cast<std::uint32_t>(static_cast<std::int16_t>(word(v - 2)));
        } else if (w6 == 0x4EB9) {
            callPc = v - 6;
            entry = read(CpuId::M68k, v - 4, 4) & 0xFFFFFF;
        }
        if (!callPc) {
            continue;
        }
        CallFrame f;
        f.kind = CallFrame::Kind::Call;
        f.callPc = *callPc;
        f.entry = entry.value_or(0) & 0xFFFFFF;
        f.returnPc = v;
        f.sp = at;
        f.verified = false;
        found.push_back(f);
        at += 2;  // skip the low word of this long
    }
    std::reverse(found.begin(), found.end());
    return found;
}

}  // namespace gen::debug
