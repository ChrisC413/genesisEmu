#include "core/m68k/m68k.hpp"

#include <memory>

#include "core/m68k/cpu_core.hpp"

namespace gen::m68k {

namespace {

constexpr int kVectorAddressError = 3;
constexpr int kVectorIllegal = 4;
constexpr int kVectorPrivilege = 8;
constexpr int kVectorLineA = 10;
constexpr int kVectorLineF = 11;
constexpr int kVectorSpurious = 24;
constexpr int kVectorAutovectorBase = 24;

void opIllegal(CpuCore& c, std::uint32_t /*op*/) {
    c.illegalInstruction(kVectorIllegal);
}
void opLineA(CpuCore& c, std::uint32_t /*op*/) {
    c.illegalInstruction(kVectorLineA);
}
void opLineF(CpuCore& c, std::uint32_t /*op*/) {
    c.illegalInstruction(kVectorLineF);
}

// Group 0 (address error) exception: 14-byte frame, 50 cycles on top of the time spent before the fault.
void addressError(CpuCore& c, const AddressErrorTrap& e, std::uint16_t opcode, int cyclesAtFault) {
    const std::uint32_t stackedPc = c.s.pc - 2;
    const std::uint16_t fc = static_cast<std::uint16_t>((c.s.supervisor() ? 4 : 0) | (e.program ? 2 : 1));
    const std::uint16_t status =
        static_cast<std::uint16_t>((opcode & 0xFFE0) | (e.read ? 0x10 : 0) | (e.program ? 0x08 : 0) | fc);
    const std::uint16_t oldSr = c.s.sr;
    c.setSr(static_cast<std::uint16_t>((c.s.sr | kSrSupervisor) & ~kSrTrace));
    try {
        c.s.a[7] -= 14;
        const std::uint32_t sp = c.s.a[7];
        c.writeWord(sp + 12, static_cast<std::uint16_t>(stackedPc));
        c.writeWord(sp + 10, static_cast<std::uint16_t>(stackedPc >> 16));
        c.writeWord(sp + 8, oldSr);
        c.writeWord(sp + 6, opcode);
        c.writeWord(sp + 4, static_cast<std::uint16_t>(e.addr));
        c.writeWord(sp + 2, static_cast<std::uint16_t>(e.addr >> 16));
        c.writeWord(sp + 0, status);
        c.jump(c.read<4>(kVectorAddressError * 4));
    } catch (const AddressErrorTrap&) {
        c.s.halted = true;  // double bus fault
    }
    c.cycles = cyclesAtFault + 50;
    c.flow = Flow::Exception;
}

}  // namespace

// ---- CpuCore exception helpers
// ------------------------------------------------------------------------------

void throwAddressError(std::uint32_t addr, bool read, bool program) {
    throw AddressErrorTrap{addr, read, program};
}

void CpuCore::exception(int vector, std::uint32_t stackedPc, int idleCycles) {
    const std::uint16_t oldSr = s.sr;
    setSr(static_cast<std::uint16_t>((s.sr | kSrSupervisor) & ~kSrTrace));
    idle(idleCycles);
    push32(stackedPc);
    push16(oldSr);
    jump(read<4>(static_cast<std::uint32_t>(vector) * 4));
    flow = Flow::Exception;
}

void CpuCore::privilegeViolation() {
    exception(kVectorPrivilege, instrPc, 6);
}

void CpuCore::illegalInstruction(int vector) {
    exception(vector, instrPc, 6);
}

// ---- Decode table
// -------------------------------------------------------------------------------------------

std::unique_ptr<DecodeTable> buildDecodeTable() {
    auto table = std::make_unique<DecodeTable>();
    for (std::uint32_t op = 0; op < 65536; ++op) {
        const std::uint32_t line = op >> 12;
        table->handlers[op] = line == 0xA ? &opLineA : line == 0xF ? &opLineF : &opIllegal;
    }
    TableBuilder b{*table};
    registerDataMovement(b);
    registerArithmetic(b);
    registerLogic(b);
    registerShiftBit(b);
    registerBcd(b);
    registerProgramControl(b);
    registerSystem(b);
    return table;
}

const DecodeTable& decodeTable() {
    static const std::unique_ptr<DecodeTable> table = buildDecodeTable();
    return *table;
}

int buildDecodeTableOnce() {
    const auto table = buildDecodeTable();
    int n = 0;
    for (const bool v : table->valid) {
        n += v ? 1 : 0;
    }
    return n;
}

bool isValidOpcode(std::uint16_t opcode) {
    return decodeTable().valid[opcode];
}

// ---- M68k
// ---------------------------------------------------------------------------------------------------

M68k::M68k(Bus68k& bus) : core_(std::make_unique<CpuCore>()) {
    core_->bus = &bus;
    core_->table = &decodeTable();
}

M68k::~M68k() = default;

int M68k::reset() {
    CpuCore& c = *core_;
    c.cycles = 0;
    c.s.stopped = false;
    c.s.halted = false;
    c.s.sr = kSrSupervisor | kSrIntMask;
    c.instrPc = 0;
    try {
        c.s.a[7] = c.read<4>(0);
        const std::uint32_t pc = c.read<4>(4);
        c.jump(pc);
        c.s.pc -= 2;
    } catch (const AddressErrorTrap&) {
        c.s.halted = true;
    }
    c.flow = Flow::Exception;
    c.cycles = 40;
    return c.cycles;
}

int M68k::step() {
    CpuCore& c = *core_;
    c.cycles = 0;
    c.broke = false;
    c.flow = Flow::Sequential;
    if (c.s.halted) {
        c.flow = Flow::Stopped;
        return 4;
    }

    const int mask = (c.s.sr >> 8) & 7;
    if (c.nmiPending || c.irqLevel > mask) {
        const int level = c.nmiPending ? 7 : c.irqLevel;
        c.nmiPending = false;
        c.s.stopped = false;
        c.instrPc = c.s.pc;
        c.s.pc += 2;
        try {
            int vector = c.bus->iack(level);
            if (vector == Bus68k::kAutovector) {
                vector = kVectorAutovectorBase + level;
            } else if (vector == Bus68k::kSpurious) {
                vector = kVectorSpurious;
            }
            if (c.hooks.onIrq != nullptr) {
                c.hooks.onIrq(c.hooks.ctx, CpuId::M68k, level);
            }
            const std::uint32_t stackedPc = c.s.pc - 2;
            const std::uint16_t oldSr = c.s.sr;
            c.setSr(static_cast<std::uint16_t>(((c.s.sr | kSrSupervisor) & ~(kSrTrace | kSrIntMask)) |
                                               (level << 8)));
            c.idle(16);  // includes the IACK cycle
            c.push32(stackedPc);
            c.push16(oldSr);
            c.jump(c.read<4>(static_cast<std::uint32_t>(vector) * 4));
            c.flow = Flow::Exception;
        } catch (const AddressErrorTrap&) {
            c.s.halted = true;
        }
        c.s.pc -= 2;
        return c.cycles;
    }

    if (c.s.stopped) {
        c.flow = Flow::Stopped;
        return 4;
    }

    if (c.hooks.onInstr != nullptr && !c.skipBreak) {
        if (c.hooks.onInstr(c.hooks.ctx, CpuId::M68k, c.s.pc, c.s.ir) == Action::Break) {
            c.broke = true;
            c.skipBreak = true;
            return 0;
        }
    }
    c.skipBreak = false;

    const std::uint16_t op = c.s.ir;
    const bool trace = (c.s.sr & kSrTrace) != 0;
    c.instrPc = c.s.pc;
    c.s.pc += 2;
    try {
        c.table->handlers[op](c, op);
        if (trace && c.flow != Flow::Exception && !c.s.stopped) {
            c.exception(9, c.s.pc - 2, 6);
        }
    } catch (const AddressErrorTrap& e) {
        addressError(c, e, op, c.cycles);
    }
    c.cycles += c.bus->waitCycles;
    c.bus->waitCycles = 0;
    c.s.pc -= 2;
    return c.cycles;
}

bool M68k::breakHit() const {
    return core_->broke;
}

void M68k::setIrqLevel(int level) {
    level &= 7;
    if (level == 7 && core_->irqLevel != 7) {
        core_->nmiPending = true;
    }
    core_->irqLevel = level;
}

int M68k::irqLevel() const {
    return core_->irqLevel;
}

State& M68k::state() {
    return core_->s;
}

const State& M68k::state() const {
    return core_->s;
}

void M68k::setHooks(const Hooks& hooks) {
    core_->hooks = hooks;
}

void M68k::setTasWriteback(bool enabled) {
    core_->tasWriteback = enabled;
}

Flow M68k::lastFlow() const {
    return core_->flow;
}

void M68k::refillPrefetch() {
    CpuCore& c = *core_;
    const int saved = c.cycles;
    try {
        c.jump(c.s.pc);
        c.s.pc -= 2;
    } catch (const AddressErrorTrap&) {
        c.s.ir = 0;
        c.s.irc = 0;
    }
    c.cycles = saved;
}

}  // namespace gen::m68k
