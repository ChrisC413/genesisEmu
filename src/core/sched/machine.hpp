#pragma once
// The whole console: 68k, bus, I/O, Z80 control, VDP, driven by the master-clock scanline scheduler
// (spec 01 "Timing and scheduler", M02-05).

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "core/audio/sound.hpp"
#include "core/bus/io.hpp"
#include "core/bus/system_bus.hpp"
#include "core/cart/cart_device.hpp"
#include "core/cart/sram.hpp"
#include "core/hooks.hpp"
#include "core/m68k/m68k.hpp"
#include "core/util/hash.hpp"
#include "core/vdp/vdp.hpp"
#include "core/z80/z80.hpp"
#include "core/z80/z80_control.hpp"

namespace gen::state {
class Archive;
struct StateFile;
}  // namespace gen::state

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
    enum class Kind : std::uint8_t { Frames, Instructions, Mclk, Lines, UntilBreak };
    Kind kind = Kind::Frames;
    std::uint64_t count = 1;

    static RunLimit frames(std::uint64_t n) { return {Kind::Frames, n}; }
    static RunLimit instructions(std::uint64_t n) { return {Kind::Instructions, n}; }
    static RunLimit mclk(std::uint64_t n) { return {Kind::Mclk, n}; }
    // Until `n` scanline boundaries have passed (the 68k stops at the first instruction of the new line).
    static RunLimit lines(std::uint64_t n) { return {Kind::Lines, n}; }
    static RunLimit untilBreak() { return {Kind::UntilBreak, 0}; }
};

enum class StopReason : std::uint8_t {
    LimitReached,
    Breakpoint,   // onInstr returned Break (on RunResult::cpu); that instruction has not executed
    Watchpoint,   // requestStop(): stopped after the 68k instruction during which the request was made
    Exception,    // a fault exception was taken and MachineConfig::stopOnFault is set
    Unsupported,  // reserved
};
const char* toString(StopReason reason);

struct RunResult {
    StopReason reason = StopReason::LimitReached;
    std::uint64_t instructions = 0;  // 68k instructions (and exception entries) executed
    int vector = -1;                 // for StopReason::Exception
    CpuId cpu = CpuId::M68k;         // the CPU that broke (Breakpoint) or the request named (Watchpoint)
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
    // The console's reset button: only the 68k takes its reset exception (SSP and PC from the vectors);
    // memory, the VDP, the Z80 side and the sound chips keep running state.
    void softReset();

    RunResult run(RunLimit limit);
    // Exactly one instruction on `cpu`; line / frame boundaries it crosses are processed. A Z80 step while
    // the Z80 is held (BUSREQ / RESET) executes nothing and reports 0 instructions.
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
    // Debugger (M06-04): make run() / step() return `reason` once the current 68k instruction is complete.
    // Called from a hook (a watchpoint in onAccess, a Z80 or VDP event); harmless outside run().
    void requestStop(StopReason reason, CpuId cpu) {
        stopRequested_ = true;
        requestedStop_ = reason;
        requestedStopCpu_ = cpu;
    }

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
    // YM2612 + PSG (M05). Samples accumulate in sound().samples() until the frontend clears them.
    audio::SoundSystem& sound() { return sound_; }
    const audio::SoundSystem& sound() const { return sound_; }
    const std::vector<std::uint8_t>& rom() const { return rom_; }
    // Debugger ROM patch (M06-06): changes one byte of the cartridge image in place (not part of a state).
    void patchRom(std::uint32_t addr, std::uint8_t value) {
        if (addr < rom_.size()) {
            rom_[addr] = value;
        }
    }
    // SHA-1 of the image as passed to the constructor (before padding); save states are tied to it.
    const Sha1Digest& romSha1() const { return romSha1_; }

    // Cartridge device chain (M04-08). The SRAM controller is created from the header's SRAM declaration
    // (for a lock-on image, the locked-on cartridge's header) and is null when none is declared.
    void addCartDevice(std::unique_ptr<cart::ICartDevice> device);
    cart::SramDevice* sram() { return sram_; }
    const cart::SramDevice* sram() const { return sram_; }

    const MachineStats& stats() const { return stats_; }

    // XXH3-64 over every component's state (CPU, RAM, VDP, I/O, Z80 side, scheduler).
    std::uint64_t stateHash() const;

    // Save states (spec 08 "Save states", M06-00a): the full machine as a chunked, versioned file (format in
    // core/state/save_state.hpp). Call between run() / step() calls, i.e. at an instruction boundary.
    // `timestamp` is stored as given (seconds since the epoch; 0 keeps a state byte-for-byte reproducible).
    // Hooks, statistics, the bus issue log and the sound register log are not part of a state.
    // `thumbnail` adds the optional THMB chunk (a 160x112 picture of the screen) for state browsers.
    std::vector<std::uint8_t> saveState(std::uint64_t timestamp = 0, bool thumbnail = false);
    // Restores a state saved from the same ROM image (SHA-1) and region. A state is refused when it comes
    // from another ROM, has a chunk this build does not know or a newer chunk version, is corrupt, or does
    // not reproduce the state hash recorded when it was saved; the machine is then unchanged and `error`
    // says why.
    bool loadState(std::span<const std::uint8_t> bytes, std::string& error);

private:
    // false when a breakpoint / stop request stopped it. kHooked: hooks are installed, so a hook may have
    // requested a stop (without hooks the check is compiled out of the run loop).
    template <bool kHooked>
    bool stepCpu(RunResult& result);
    template <bool kHooked>
    RunResult runLoop(RunLimit limit);
    void endLine();
    void checkVint();
    void catchUp(std::uint64_t mclk);
    void runZ80(std::uint64_t mclk);
    static void syncZ80(void* ctx);
    static std::uint64_t soundNow(void* ctx);
    void serializeChunk(std::string_view tag, state::Archive& ar);
    std::string applyState(const state::StateFile& file);  // empty on success

    std::vector<std::uint8_t> rom_;
    Sha1Digest romSha1_{};
    MachineConfig config_;
    bus::Region region_ = bus::Region::Americas;
    bool pal_ = false;
    vdp::Vdp vdp_;
    bus::Io io_;
    z80::Z80Control z80_;
    audio::SoundSystem sound_;
    bus::BusClock clock_;
    std::unique_ptr<bus::SystemBus> bus_;
    std::unique_ptr<m68k::M68k> cpu_;
    std::unique_ptr<z80::Z80> z80cpu_;
    std::uint64_t z80Mclk_ = 0;
    std::uint64_t vintAt_ = 0;  // master clock the armed V-int is raised at (see endLine)
    bool inZ80_ = false;
    bool stopRequested_ = false;
    bool hooked_ = false;  // any hook installed
    StopReason requestedStop_ = StopReason::Watchpoint;
    CpuId requestedStopCpu_ = CpuId::M68k;
    Hooks hooks_;
    std::uint64_t frame_ = 0;
    int line_ = 0;
    MachineStats stats_;
    std::array<std::uint16_t, 2> pendingInput_{};
    std::vector<std::unique_ptr<cart::ICartDevice>> devices_;
    cart::SramDevice* sram_ = nullptr;
};

}  // namespace gen
