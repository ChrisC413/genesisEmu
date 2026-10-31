#pragma once
// The whole console: 68k, bus, I/O, Z80 control, VDP, driven by the master-clock scanline scheduler
// (spec 01 "Timing and scheduler", M02-05).

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "core/bus/io.hpp"
#include "core/bus/system_bus.hpp"
#include "core/hooks.hpp"
#include "core/m68k/m68k.hpp"
#include "core/vdp/vdp.hpp"
#include "core/z80/z80.hpp"
#include "core/z80/z80_control.hpp"

namespace gen {

inline constexpr int kMclkPerLine = 3420;
inline constexpr int kLinesNtsc = 262;
inline constexpr int kLinesPal = 313;
inline constexpr int kM68kDivider = 7;
inline constexpr int kZ80Divider = 15;
inline constexpr std::uint64_t kMclkPerFrameNtsc = static_cast<std::uint64_t>(kMclkPerLine) * kLinesNtsc;

struct MachineConfig {
    std::optional<bus::Region> region;  // default: from the cartridge header
    bool tmss = false;                  // model a TMSS console (VDP locked until 'SEGA' is written)
    bool stopOnFault = false;           // run() stops with StopReason::Exception on a fault vector
    z80::WindowTiming z80Window;        // cost of Z80 accesses to the 68k bank window
};

struct RunLimit {
    enum class Kind : std::uint8_t { Frames, Instructions, Mclk, UntilBreak };
    Kind kind = Kind::Frames;
    std::uint64_t count = 1;

    static RunLimit frames(std::uint64_t n) { return {Kind::Frames, n}; }
    static RunLimit instructions(std::uint64_t n) { return {Kind::Instructions, n}; }
    static RunLimit mclk(std::uint64_t n) { return {Kind::Mclk, n}; }
    static RunLimit untilBreak() { return {Kind::UntilBreak, 0}; }
};

enum class StopReason : std::uint8_t {
    LimitReached,
    Breakpoint,   // onInstr returned Break; the instruction has not executed
    Watchpoint,   // reserved (M06-04)
    Exception,    // a fault exception was taken and MachineConfig::stopOnFault is set
    Unsupported,  // reserved
};
const char* toString(StopReason reason);

struct RunResult {
    StopReason reason = StopReason::LimitReached;
    std::uint64_t instructions = 0;  // 68k instructions (and exception entries) executed
    int vector = -1;                 // for StopReason::Exception
};

// True for exception vectors that indicate a program error: bus/address error, illegal, divide by zero,
// CHK, TRAPV, privilege, trace, line A/F, reserved / uninitialized vectors and spurious interrupts.
bool isFaultVector(int vector);

// Cheap counters the scheduler keeps for tests and the CLI (not part of the state hash).
struct MachineStats {
    std::array<std::uint64_t, 256> exceptions{};  // per vector, excluding reset
    std::uint64_t faults = 0;
    int firstFaultVector = -1;
    std::uint32_t firstFaultPc = 0;
    std::uint64_t vints = 0;       // level-6 interrupts taken
    std::uint32_t lastVintPc = 0;  // interrupted PC of the most recent V-int
    std::uint32_t vintsThisFrame = 0;
    std::uint32_t vintsLastFrame = 0;  // V-ints taken during the last completed frame
};

class Machine {
public:
    // Takes the de-interleaved ROM image (up to 4 MB) and performs a hard reset.
    Machine(std::vector<std::uint8_t> rom, std::uint8_t headerRegions, const MachineConfig& config = {});
    ~Machine();
    Machine(const Machine&) = delete;
    Machine& operator=(const Machine&) = delete;

    void hardReset();

    RunResult run(RunLimit limit);
    // Exactly one instruction on `cpu`; line / frame boundaries it crosses are processed. A Z80 step while the
    // Z80 is held (BUSREQ / RESET) executes nothing and reports 0 instructions.
    RunResult step(CpuId cpu);

    // Controller input (M04-01): the buttons (bus::PadButton mask) of pad `port` (0 or 1). Latched into the
    // I/O chip once per frame, at the start of the next frame, so a run is deterministic (spec 07).
    void setInput(int port, std::uint16_t buttons) {
        pendingInput_[static_cast<std::size_t>(port & 1)] = buttons;
    }
    std::uint16_t input(int port) const { return pendingInput_[static_cast<std::size_t>(port & 1)]; }
    void setPadType(int port, bus::PadType type) { io_.setPadType(port, type); }

    void setHooks(const Hooks& hooks);
    const Hooks& hooks() const { return hooks_; }

    std::uint64_t frame() const { return frame_; }
    int line() const { return line_; }
    int linesPerFrame() const { return pal_ ? kLinesPal : kLinesNtsc; }
    std::uint64_t lineStartMclk() const { return clock_.lineStart; }
    std::uint64_t cpuMclk() const { return clock_.cpuMclk; }  // may run ahead of the line (overshoot)
    bool pal() const { return pal_; }
    bus::Region region() const { return region_; }

    m68k::M68k& cpu() { return *cpu_; }
    const m68k::M68k& cpu() const { return *cpu_; }
    bus::SystemBus& bus() { return *bus_; }
    const bus::SystemBus& bus() const { return *bus_; }
    vdp::Vdp& vdp() { return vdp_; }
    const vdp::Vdp& vdp() const { return vdp_; }
    bus::Io& io() { return io_; }
    z80::Z80Control& z80() { return z80_; }
    const z80::Z80Control& z80() const { return z80_; }
    z80::Z80& z80Cpu() { return *z80cpu_; }
    const z80::Z80& z80Cpu() const { return *z80cpu_; }
    std::uint64_t z80Mclk() const { return z80Mclk_; }  // master clock the Z80 has run up to
    const std::vector<std::uint8_t>& rom() const { return rom_; }

    const MachineStats& stats() const { return stats_; }

    // XXH3-64 over every component's state (CPU, RAM, VDP, I/O, Z80 side, scheduler).
    std::uint64_t stateHash() const;

private:
    bool stepCpu(RunResult& result);  // false when a breakpoint stopped it
    void endLine();
    void catchUp(std::uint64_t mclk);
    void runZ80(std::uint64_t mclk);
    static void syncZ80(void* ctx);

    std::vector<std::uint8_t> rom_;
    MachineConfig config_;
    bus::Region region_ = bus::Region::Americas;
    bool pal_ = false;
    vdp::Vdp vdp_;
    bus::Io io_;
    z80::Z80Control z80_;
    bus::BusClock clock_;
    std::unique_ptr<bus::SystemBus> bus_;
    std::unique_ptr<m68k::M68k> cpu_;
    std::unique_ptr<z80::Z80> z80cpu_;
    std::uint64_t z80Mclk_ = 0;
    bool inZ80_ = false;
    Hooks hooks_;
    std::uint64_t frame_ = 0;
    int line_ = 0;
    MachineStats stats_;
    std::array<std::uint16_t, 2> pendingInput_{};
};

}  // namespace gen
