#include "core/sched/machine.hpp"

#include <algorithm>

#include "core/state/state_hash.hpp"

namespace gen {

namespace {
constexpr int kVintVector = 24 + 6;
}

const char* toString(StopReason reason) {
    switch (reason) {
    case StopReason::LimitReached:
        return "limit";
    case StopReason::Breakpoint:
        return "breakpoint";
    case StopReason::Watchpoint:
        return "watchpoint";
    case StopReason::Exception:
        return "exception";
    case StopReason::Unsupported:
        return "unsupported";
    }
    return "?";
}

bool isFaultVector(int vector) {
    return vector >= 2 && vector <= 24;
}

Machine::Machine(std::vector<std::uint8_t> rom, std::uint8_t headerRegions, const MachineConfig& config)
    : rom_(std::move(rom)), config_(config) {
    // Pad to whole 64 KB pages so every ROM page can be a direct fast page.
    rom_.resize(std::min<std::size_t>((rom_.size() + 0xFFFF) & ~std::size_t{0xFFFF}, 0x400000), 0xFF);
    region_ = config.region.value_or(bus::regionFromHeader(headerRegions));
    pal_ = region_ == bus::Region::Europe;
    bus_ = std::make_unique<bus::SystemBus>(vdp_, io_, z80_, clock_);
    cpu_ = std::make_unique<m68k::M68k>(*bus_);
    bus_->attach(rom_, *cpu_, config_.tmss);
    vdp_.setHooks(&hooks_);
    hardReset();
}

Machine::~Machine() = default;

void Machine::hardReset() {
    clock_ = bus::BusClock{};
    frame_ = 0;
    line_ = 0;
    stats_ = MachineStats{};
    bus_->reset();
    bus_->clearIssues();
    io_.reset(region_, pal_, config_.tmss);
    z80_.reset();
    vdp_.reset(pal_);
    cpu_->state() = m68k::State{};
    cpu_->setIrqLevel(0);
    const int cycles = cpu_->reset();
    clock_.cpuMclk = static_cast<std::uint64_t>(cycles) * kM68kDivider;
    vdp_.beginLine(0);
    bus_->updateIrq();
}

void Machine::setHooks(const Hooks& hooks) {
    hooks_ = hooks;
    cpu_->setHooks(hooks_);
}

void Machine::catchUp(std::uint64_t /*mclk*/) {
    // Z80 (M04) and audio chips (M05) run up to `mclk` here.
}

void Machine::endLine() {
    const std::uint64_t lineEnd = clock_.lineStart + kMclkPerLine;
    catchUp(lineEnd);
    clock_.lineStart = lineEnd;
    if (++line_ >= linesPerFrame()) {
        line_ = 0;
        ++frame_;
        stats_.vintsLastFrame = stats_.vintsThisFrame;
        stats_.vintsThisFrame = 0;
        if (hooks_.onFrameEnd != nullptr) {
            hooks_.onFrameEnd(hooks_.ctx, frame_ - 1);
        }
    }
    vdp_.beginLine(line_);
    bus_->updateIrq();
}

bool Machine::stepCpu(RunResult& result) {
    const std::uint32_t pc = cpu_->state().pc;
    const int cycles = cpu_->step();
    if (cpu_->breakHit()) {
        result.reason = StopReason::Breakpoint;
        return false;
    }
    ++result.instructions;
    clock_.cpuMclk += static_cast<std::uint64_t>(cycles) * kM68kDivider;
    const int vector = cpu_->lastException();
    if (vector >= 0) {
        ++stats_.exceptions[static_cast<std::size_t>(vector & 0xFF)];
        if (vector == kVintVector) {
            ++stats_.vints;
            ++stats_.vintsThisFrame;
            stats_.lastVintPc = pc;
        } else if (isFaultVector(vector)) {
            if (stats_.faults++ == 0) {
                stats_.firstFaultVector = vector;
                stats_.firstFaultPc = pc;
            }
            if (config_.stopOnFault) {
                result.reason = StopReason::Exception;
                result.vector = vector;
            }
        }
    }
    while (clock_.cpuMclk >= clock_.lineStart + kMclkPerLine) {
        endLine();
    }
    return result.reason != StopReason::Exception;
}

RunResult Machine::run(RunLimit limit) {
    RunResult result;
    const std::uint64_t targetFrame = frame_ + limit.count;
    const std::uint64_t targetMclk = clock_.cpuMclk + limit.count;
    for (;;) {
        switch (limit.kind) {
        case RunLimit::Kind::Frames:
            if (frame_ >= targetFrame) {
                return result;
            }
            break;
        case RunLimit::Kind::Instructions:
            if (result.instructions >= limit.count) {
                return result;
            }
            break;
        case RunLimit::Kind::Mclk:
            if (clock_.cpuMclk >= targetMclk) {
                return result;
            }
            break;
        case RunLimit::Kind::UntilBreak:
            break;
        }
        if (!stepCpu(result)) {
            return result;
        }
    }
}

RunResult Machine::step(CpuId cpu) {
    RunResult result;
    if (cpu != CpuId::M68k) {
        result.reason = StopReason::Unsupported;  // the Z80 executes from M04
        return result;
    }
    stepCpu(result);
    return result;
}

std::uint64_t Machine::stateHash() const {
    state::StateHasher h;
    const m68k::State& c = cpu_->state();
    h.tag("M68K");
    for (const std::uint32_t r : c.d) {
        h.u32(r);
    }
    for (const std::uint32_t r : c.a) {
        h.u32(r);
    }
    h.u32(c.usp);
    h.u32(c.ssp);
    h.u32(c.pc);
    h.u16(c.sr);
    h.u16(c.ir);
    h.u16(c.irc);
    h.boolean(c.stopped);
    h.boolean(c.halted);
    h.i32(cpu_->irqLevel());

    const bus::BusState& b = bus_->state();
    h.tag("BUS ");
    h.bytes(b.ram);
    h.u32(b.tmssRegister);
    h.u8(b.sramControl);

    const vdp::VdpState& v = vdp_.state();
    h.tag("VDP ");
    h.bytes(v.regs);
    h.bytes(v.vram);
    for (const std::uint16_t w : v.cram) {
        h.u16(w);
    }
    for (const std::uint16_t w : v.vsram) {
        h.u16(w);
    }
    h.u16(v.address);
    h.u8(v.code);
    h.boolean(v.pending);
    h.boolean(v.fillPending);
    h.boolean(v.vintPending);
    h.boolean(v.hintPending);
    h.boolean(v.spriteOverflow);
    h.boolean(v.spriteCollision);
    h.boolean(v.oddFrame);
    h.boolean(v.pal);
    h.i32(v.hintCounter);
    h.i32(v.line);

    const bus::IoState& io = io_.state();
    h.tag("IO  ");
    h.u8(io.version);
    h.bytes(io.data);
    h.bytes(io.control);
    h.bytes(io.serial);
    h.u16(io.buttons[0]);
    h.u16(io.buttons[1]);

    const z80::Z80ControlState& z = z80_.state();
    h.tag("Z80 ");
    h.bytes(z.ram);
    h.boolean(z.busRequested);
    h.boolean(z.resetAsserted);
    h.u16(z.bank);
    h.u8(z.ymAddress[0]);
    h.u8(z.ymAddress[1]);
    h.bytes(z.ymRegs[0]);
    h.bytes(z.ymRegs[1]);
    h.u8(z.psgLast);

    h.tag("SCHD");
    h.u64(frame_);
    h.i32(line_);
    h.u64(clock_.lineStart);
    h.u64(clock_.cpuMclk);
    return h.digest();
}

}  // namespace gen
