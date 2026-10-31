#include "instrument/trace_writer.hpp"

#include <cinttypes>
#include <cstdio>

#include "core/sched/machine.hpp"

namespace gen::instrument {

namespace {

constexpr std::size_t kFlushBytes = 1 << 20;
constexpr std::size_t kWordsColumn = 24;
constexpr std::size_t kDisasmColumn = 34;
constexpr std::size_t kMaxWritesShown = 16;

void appendf(std::string& out, const char* fmt, auto... args) {
    char buf[64];
    const int n = std::snprintf(buf, sizeof buf, fmt, args...);
    if (n > 0) {
        out.append(buf,
                   static_cast<std::size_t>(n) < sizeof buf ? static_cast<std::size_t>(n) : sizeof buf - 1);
    }
}

void pad(std::string& s, std::size_t from, std::size_t width) {
    if (s.size() < from + width) {
        s.append(from + width - s.size(), ' ');
    }
}

}  // namespace

TraceWriter::TraceWriter(Machine& machine, std::ostream* out, TraceConfig config, DisassembleFn disassemble)
    : machine_(machine), out_(out), config_(config), disassemble_(std::move(disassemble)) {
    if (config_.firstExecutionOnly) {
        seen_.assign(0x1000000 / 8, 0);
    }
    buffer_.reserve(kFlushBytes + 4096);
    buffer_ += kHeader;
    buffer_ += "\n# frame line mclk cpu pc words disassembly | register deltas | memory writes\n";
}

TraceWriter::~TraceWriter() {
    finish();
}

void TraceWriter::attach() {
    Hooks h = machine_.hooks();
    h.ctx = this;
    h.onInstr = &TraceWriter::onInstr;
    h.onAccess = &TraceWriter::onAccess;
    h.onIrq = &TraceWriter::onIrq;
    machine_.setHooks(h);
}

void TraceWriter::finish() {
    complete();
    flush();
    if (machine_.hooks().ctx == this) {
        machine_.setHooks(Hooks{});
    }
}

void TraceWriter::flush() {
    if (!buffer_.empty() && out_ != nullptr) {
        out_->write(buffer_.data(), static_cast<std::streamsize>(buffer_.size()));
        out_->flush();
    }
    buffer_.clear();
}

TraceWriter::Regs TraceWriter::snapshot() const {
    const m68k::State& s = machine_.cpu().state();
    Regs r;
    r.d = s.d;
    r.a = s.a;
    r.otherSp = s.supervisor() ? s.usp : s.ssp;
    r.sr = s.sr;
    return r;
}

bool TraceWriter::frameInRange() const {
    const std::uint64_t f = machine_.frame();
    return f >= config_.startFrame && f < config_.endFrame && records_ < config_.maxRecords;
}

void TraceWriter::begin(std::uint32_t pc, int irqLevel) {
    open_ = true;
    before_ = snapshot();
    writes_.clear();
    line_.clear();
    appendf(line_, "%" PRIu64 " %3d %10" PRIu64 " M68K %06X  ", machine_.frame(), machine_.line(),
            machine_.cpuMclk(), pc);
    const std::size_t wordsAt = line_.size();
    std::string text;
    if (irqLevel > 0) {
        appendf(text, "interrupt level %d", irqLevel);
    } else {
        // Instruction bytes from side-effect-free memory (ROM / RAM pages).
        std::array<std::uint8_t, 10> bytes{};
        std::size_t n = 0;
        const auto& bus = machine_.bus();
        for (; n < bytes.size(); ++n) {
            const std::uint32_t a = (pc + static_cast<std::uint32_t>(n)) & 0xFFFFFF;
            const std::uint8_t* page = bus.fastRead[a >> 16];
            if (page == nullptr) {
                break;
            }
            bytes[n] = page[a & 0xFFFF];
        }
        int length = 2;
        if (n >= 2 && disassemble_) {
            text = disassemble_(std::span<const std::uint8_t>(bytes.data(), n), pc, length);
        } else {
            const std::uint16_t op = machine_.cpu().state().ir;
            bytes[0] = static_cast<std::uint8_t>(op >> 8);
            bytes[1] = static_cast<std::uint8_t>(op);
            n = 2;
            text = "?";
        }
        for (int i = 0; i + 1 < length && static_cast<std::size_t>(i + 1) < n; i += 2) {
            appendf(line_, "%02X%02X ", bytes[static_cast<std::size_t>(i)],
                    bytes[static_cast<std::size_t>(i + 1)]);
        }
    }
    pad(line_, wordsAt, kWordsColumn);
    const std::size_t textAt = line_.size();
    for (char& ch : text) {
        ch = ch == '\t' ? ' ' : ch;  // tabs would break the columns
    }
    line_ += text;
    pad(line_, textAt, kDisasmColumn);
}

void TraceWriter::complete() {
    if (!open_) {
        return;
    }
    open_ = false;
    ++records_;
    const Regs after = snapshot();
    buffer_ += line_;
    buffer_ += " |";
    bool any = false;
    for (std::size_t i = 0; i < 8; ++i) {
        if (after.d[i] != before_.d[i]) {
            appendf(buffer_, " D%zu=%08X", i, after.d[i]);
            any = true;
        }
    }
    for (std::size_t i = 0; i < 8; ++i) {
        if (after.a[i] != before_.a[i]) {
            appendf(buffer_, " A%zu=%08X", i, after.a[i]);
            any = true;
        }
    }
    if (after.otherSp != before_.otherSp || (after.sr ^ before_.sr) & m68k::kSrSupervisor) {
        appendf(buffer_, " %s=%08X", (after.sr & m68k::kSrSupervisor) != 0 ? "USP" : "SSP", after.otherSp);
        any = true;
    }
    if (after.sr != before_.sr) {
        appendf(buffer_, " SR=%04X", after.sr);
        any = true;
    }
    if (!any) {
        buffer_ += " -";
    }
    buffer_ += " |";
    for (std::size_t i = 0; i < writes_.size() && i < kMaxWritesShown; ++i) {
        const Write& w = writes_[i];
        const char sz = w.size == 1 ? 'B' : w.size == 2 ? 'W' : 'L';
        const int digits = w.size * 2;
        appendf(buffer_, " W.%c %06X=%0*X", sz, w.addr, digits, w.value);
    }
    if (writes_.size() > kMaxWritesShown) {
        appendf(buffer_, " +%zu", writes_.size() - kMaxWritesShown);
    }
    if (writes_.empty()) {
        buffer_ += " -";
    }
    buffer_ += '\n';
    if (buffer_.size() >= kFlushBytes) {
        flush();
    }
}

Action TraceWriter::onInstr(void* ctx, CpuId, std::uint32_t pc, std::uint16_t) {
    auto& t = *static_cast<TraceWriter*>(ctx);
    t.complete();
    if (!t.frameInRange()) {
        return Action::Continue;
    }
    if (t.config_.pcRange && (pc < t.config_.pcRange->first || pc > t.config_.pcRange->second)) {
        return Action::Continue;
    }
    if (t.config_.firstExecutionOnly) {
        std::uint8_t& bits = t.seen_[(pc & 0xFFFFFF) >> 3];
        const auto bit = static_cast<std::uint8_t>(1u << (pc & 7));
        if ((bits & bit) != 0) {
            return Action::Continue;
        }
        bits |= bit;
    }
    t.begin(pc, 0);
    return Action::Continue;
}

void TraceWriter::onAccess(void* ctx, CpuId, AccessKind kind, std::uint32_t addr, std::uint8_t size,
                           std::uint32_t value, std::uint32_t) {
    auto& t = *static_cast<TraceWriter*>(ctx);
    if (t.open_ && kind == AccessKind::Write) {
        t.writes_.push_back(Write{addr, value, size});
    }
}

void TraceWriter::onIrq(void* ctx, CpuId, int level) {
    auto& t = *static_cast<TraceWriter*>(ctx);
    t.complete();
    if (t.frameInRange() && !t.config_.pcRange) {
        t.begin(t.machine_.cpu().currentPc(), level);
    }
}

}  // namespace gen::instrument
