#pragma once
// Headless debugger core (spec 08, milestone M06): run control and stepping on both CPUs, execution
// breakpoints with conditions / hit counts / logpoints, memory, VDP, DMA, interrupt, exception and frame
// watchpoints, trace from pause, the instruction ring buffer and the 68k shadow call stack.
//
// The debugger is the single owner of the Machine's hooks. It installs only the callbacks the active
// features need and clears them when nothing is active, so a machine with an idle debugger runs exactly as
// without one (spec 01: a null hook costs one predictable branch). Nothing here is part of a save state,
// and observing never changes emulation results: only stops, register / memory edits and ROM patches do.

#include <array>
#include <cstdint>
#include <deque>
#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <vector>

#include "core/sched/machine.hpp"
#include "debug/expr.hpp"

namespace gen::instrument {
class TraceWriter;
}

namespace gen::debug {

enum class BpKind : std::uint8_t {
    Exec,    // [start, end] instruction addresses on `cpu`
    Read,    // memory reads in [start, end] on `cpu`
    Write,   // memory writes
    Access,  // reads and writes
    VdpReg,  // VDP register numbers [start, end] written
    Vram,    // VRAM / CRAM / VSRAM byte addresses [start, end] written (data port or 68k DMA)
    Cram,
    Vsram,
    Dma,        // a DMA starts whose destination is in `dmaTarget` (and [start, end])
    Interrupt,  // the 68k (or Z80) accepts an interrupt of level [start, end]
    Exception,  // the 68k enters an exception vector in [start, end] (includes traps and interrupts)
    Frame,      // frame number [start, end] begins
};
const char* toString(BpKind kind);

struct Breakpoint {
    int id = 0;
    BpKind kind = BpKind::Exec;
    CpuId cpu = CpuId::M68k;
    std::uint32_t start = 0;
    std::uint32_t end = 0;  // inclusive; == start for a single address
    bool enabled = true;
    bool temporary = false;         // removed after the first stop
    std::string condition;          // expression (expr.hpp); empty = always
    std::uint64_t ignoreHits = 0;   // the first N matching hits only count
    std::uint8_t sizes = 0x7;       // memory watchpoints: bit 0 = byte, 1 = word (68k longs are two words)
    int dmaTarget = -1;             // Dma: vdp::DmaTarget value, or -1 for any
    bool log = false;               // logpoint: append the message and continue
    std::string message;            // logpoint format (expr.hpp Format)
    bool stop = true;               // a logpoint may also stop
    std::uint32_t traceFrames = 0;  // on hit: begin a trace for this many frames (needs a trace path)
    std::uint64_t hits = 0;         // matching hits so far (after the condition)

    // Compiled forms, built by Debugger::add / update.
    std::optional<Expr> compiledCondition;
    std::optional<Format> compiledMessage;
};

// Why the machine stopped last (shown in the status bar).
struct StopInfo {
    enum class Kind : std::uint8_t { None, Pause, Step, Breakpoint, Watchpoint, Limit, Fault };
    Kind kind = Kind::None;
    CpuId cpu = CpuId::M68k;
    std::uint32_t pc = 0;       // the stopped CPU's PC (for a watchpoint: the accessing instruction)
    int breakpoint = -1;        // id
    std::uint32_t address = 0;  // watchpoint: accessed address
    std::uint32_t value = 0;
    std::string text;  // human readable summary
};

// One frame of the 68k call stack.
struct CallFrame {
    enum class Kind : std::uint8_t { Call, Interrupt, Exception };
    Kind kind = Kind::Call;
    std::uint32_t callPc = 0;    // the JSR / BSR, or the interrupted PC
    std::uint32_t entry = 0;     // the subroutine / handler address
    std::uint32_t returnPc = 0;  // where it returns to
    std::uint32_t sp = 0;        // SP right after the return address / exception frame was pushed
    int vector = -1;             // Interrupt / Exception
    bool verified = true;        // stack walk: false when the call site could not be confirmed
};
std::string describeVector(int vector);

// One entry of the instruction ring buffer.
struct RingEntry {
    std::uint64_t mclk = 0;
    std::uint32_t frame = 0;
    std::uint32_t pc = 0;
    std::uint16_t line = 0;
    CpuId cpu = CpuId::M68k;
    std::int8_t irq = 0;  // > 0: interrupt entry of this level (68k)
};

struct TraceOptions {
    std::optional<std::pair<std::uint32_t, std::uint32_t>> pcRange;
    bool firstExecutionOnly = false;
    std::uint64_t maxRecords = UINT64_MAX;
    std::uint64_t frames = 0;  // 0 = until stopped
};

class Debugger {
public:
    explicit Debugger(Machine& machine);
    ~Debugger();
    Debugger(const Debugger&) = delete;
    Debugger& operator=(const Debugger&) = delete;

    Machine& machine() { return m_; }

    // ---- Run control (M06-01 / M06-02). Every call resumes from an instruction boundary; resuming never
    // breaks on the instruction it is paused at.
    RunResult run(RunLimit limit);
    RunResult stepInto(CpuId cpu);
    // Starts a step over / out / run-to: the next run() stops when it completes. Step over a non-call
    // instruction is a step into and completes at once (returns false); otherwise returns true.
    bool beginStepOver(CpuId cpu);
    void beginStepOut(CpuId cpu);
    void beginRunTo(CpuId cpu, std::uint32_t addr);
    void cancelStep();
    bool stepPending() const { return step_.mode != StepMode::None; }
    // Synchronous forms (tests, CLI): run up to `maxFrames` frames.
    RunResult stepOver(CpuId cpu, std::uint64_t maxFrames = 600);
    RunResult stepOut(CpuId cpu, std::uint64_t maxFrames = 600);
    RunResult runTo(CpuId cpu, std::uint32_t addr, std::uint64_t maxFrames = 600);
    void notePause();  // the frontend paused between frames
    const StopInfo& lastStop() const { return stop_; }

    // ---- Breakpoints (M06-03 / M06-04). add() compiles the condition and message; on error returns -1.
    int add(Breakpoint bp, std::string& error);
    bool update(const Breakpoint& bp, std::string& error);  // by id
    bool remove(int id);
    void clearBreakpoints();
    Breakpoint* find(int id);
    const std::vector<Breakpoint>& breakpoints() const { return bps_; }
    // Exec breakpoint at exactly `addr`: removes one if present, otherwise adds one. Returns the new state.
    bool toggleExec(CpuId cpu, std::uint32_t addr);
    bool hasExec(CpuId cpu, std::uint32_t addr) const;
    void setEnabled(int id, bool enabled);

    // Logpoint output (most recent last).
    const std::deque<std::string>& log() const { return log_; }
    void clearLog() { log_.clear(); }

    // ---- Trace from pause (M06-09): the M02-06 text format, 68k only.
    bool beginTrace(const std::string& path, const TraceOptions& options, std::string& error);
    void endTrace();
    bool tracing() const { return trace_ != nullptr; }
    std::uint64_t traceRecords() const;
    const std::string& tracePath() const { return tracePath_; }
    void setTraceOnHitPath(std::string path) { traceOnHitPath_ = std::move(path); }

    // Ring buffer: the last `capacity` instructions of both CPUs. Off by default (D-030).
    void setRingEnabled(bool enabled, std::size_t capacity = 100000);
    bool ringEnabled() const { return ringEnabled_; }
    std::vector<RingEntry> ring() const;  // oldest first
    // Writes the ring as text (one line per instruction with disassembly), oldest first.
    void dumpRing(std::ostream& out) const;

    // ---- Call stack (M06-11). Shadow tracking follows JSR / BSR / interrupts exactly while enabled;
    // callStack() falls back to a heuristic stack walk when it is off.
    void setCallStackTracking(bool enabled);
    bool callStackTracking() const { return callTracking_; }
    std::vector<CallFrame> callStack() const;  // innermost last
    std::vector<CallFrame> walkStack(int maxFrames = 32) const;

    // Hook state, for tests and the performance check: true when any callback is installed.
    bool hooksInstalled() const;

private:
    enum class StepMode : std::uint8_t { None, Over, Out, RunTo };
    struct StepState {
        StepMode mode = StepMode::None;
        CpuId cpu = CpuId::M68k;
        std::uint32_t target = 0;
        std::uint32_t sp = 0;
        bool lastWasReturn = false;
    };
    struct Skip {
        std::uint32_t pc = 0;
        bool armed = false;
    };
    class Context;

    static Action onInstr(void* ctx, CpuId cpu, std::uint32_t pc, std::uint16_t opcode);
    static void onAccess(void* ctx, CpuId cpu, AccessKind kind, std::uint32_t addr, std::uint8_t size,
                         std::uint32_t value, std::uint32_t pc);
    static void onDma(void* ctx, const DmaEvent& event);
    static void onIrq(void* ctx, CpuId cpu, int level);
    static void onException(void* ctx, CpuId cpu, int vector, std::uint32_t pc);
    static void onVdpWrite(void* ctx, VdpWriteKind kind, std::uint32_t addr, std::uint16_t value);
    static void onFrameEnd(void* ctx, std::uint64_t frame);

    Action instr(CpuId cpu, std::uint32_t pc, std::uint16_t opcode);
    // A matching event for `bp`: evaluates the condition, counts, logs, and decides whether to stop.
    bool hit(Breakpoint& bp, CpuId cpu, std::uint32_t addr, std::uint32_t value);
    void stopAt(StopInfo::Kind kind, const Breakpoint* bp, CpuId cpu, std::uint32_t pc, std::uint32_t addr,
                std::uint32_t value);
    void requestStop(StopInfo::Kind kind, const Breakpoint* bp, CpuId cpu, std::uint32_t pc,
                     std::uint32_t addr, std::uint32_t value);
    bool stepDone(CpuId cpu, std::uint32_t pc);
    void armSkips();
    void finishRun(const RunResult& r);
    void rebuild();  // indexes and hooks after a breakpoint change
    void refreshHooks();
    void trackCalls(std::uint32_t pc);
    struct PendingCall {
        bool valid = false;
        std::uint32_t pc = 0;
        std::uint32_t sp = 0;
    };
    struct PendingException {
        bool valid = false;
        int vector = -1;
        std::uint32_t pc = 0;
    };
    void applyPending(std::vector<CallFrame>& frames, PendingCall& call, PendingException& exception,
                      std::uint32_t pc) const;
    void recordRing(CpuId cpu, std::uint32_t pc, int irq);
    std::uint32_t sp(CpuId cpu) const;
    std::uint32_t pcOf(CpuId cpu) const;
    std::uint32_t read(CpuId cpu, std::uint32_t addr, int size) const;
    std::string disassemble(CpuId cpu, std::uint32_t pc, int* length = nullptr) const;
    void startTraceOnHit(std::uint32_t frames);

    Machine& m_;
    std::vector<Breakpoint> bps_;
    int nextId_ = 1;
    // Exec breakpoint index: one bit per address (68k: 16 MB / 2 since PCs are even; Z80: 64 KB).
    std::vector<std::uint64_t> execBits68k_;
    std::vector<std::uint64_t> execBitsZ80_;
    bool anyExec68k_ = false;
    bool anyExecZ80_ = false;
    // Memory watchpoint index: one bit per 256-byte page of each CPU's address space.
    std::vector<std::uint64_t> watchPages68k_;
    std::vector<std::uint64_t> watchPagesZ80_;
    bool anyWatch_ = false;
    bool anyVdpWrite_ = false;
    bool anyDma_ = false;
    bool anyIrq_ = false;
    bool anyException_ = false;
    bool anyFrame_ = false;
    bool anyZ80Event_ = false;  // a Z80 watch / interrupt break: the Z80 must freeze at its next instruction

    StepState step_;
    std::array<Skip, 2> skip_{};
    bool z80Frozen_ = false;  // a stop was requested: the Z80 does not start another one
    bool stopping_ = false;   // a stop was requested during this run
    StopInfo stop_;
    std::deque<std::string> log_;

    std::unique_ptr<instrument::TraceWriter> trace_;
    std::unique_ptr<std::ostream> traceFile_;
    std::string tracePath_;
    std::string traceOnHitPath_;

    bool ringEnabled_ = false;
    std::vector<RingEntry> ringBuf_;
    std::size_t ringNext_ = 0;
    bool ringFull_ = false;

    bool callTracking_ = false;
    std::vector<CallFrame> calls_;
    PendingCall pendingCall_;
    PendingException pendingException_;
};

}  // namespace gen::debug
