#pragma once
// Debugger panels (spec 08 "Inspectors", milestone M06): run control, disassembly, CPU registers, memory
// viewers, breakpoints, VDP viewers, trace and call stack. The panels only read the machine while it runs
// and edit it only while it is paused; run-control requests are queued and executed by the App between
// frames, so the machine is always at an instruction boundary when a panel looks at it.

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "core/hooks.hpp"
#include "debug/debugger.hpp"
#include "debug/listing.hpp"
#include "debug/memory.hpp"

namespace gen {
class Machine;
}

namespace gen::app {

inline constexpr const char* kDebuggerWindow = "Debugger";
inline constexpr const char* kDisassemblyWindow = "Disassembly";
inline constexpr const char* kM68kRegsWindow = "68k Registers";
inline constexpr const char* kZ80RegsWindow = "Z80 Registers";
inline constexpr const char* kBreakpointsWindow = "Breakpoints";
inline constexpr const char* kTraceWindow = "Trace";
inline constexpr const char* kCallStackWindow = "Call Stack";
inline constexpr const char* kVdpRegsWindow = "VDP Registers";
inline constexpr const char* kTilesWindow = "Tiles";
inline constexpr const char* kPlanesWindow = "Planes";
inline constexpr const char* kSpritesWindow = "Sprites";
inline constexpr const char* kPaletteWindow = "Palette";
inline constexpr int kMemoryViews = 4;  // "Memory 1" .. "Memory 4"

// An OpenGL RGBA texture for panel images.
class Texture {
public:
    Texture() = default;
    ~Texture();
    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;
    void update(int width, int height, const std::uint32_t* rgba);
    unsigned id() const { return tex_; }
    int width() const { return w_; }
    int height() const { return h_; }

private:
    unsigned tex_ = 0;
    int w_ = 0;
    int h_ = 0;
};

// A box drawn over the game view, in screen pixels of the emulated picture.
struct ScreenBox {
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
    std::uint32_t colour = 0xFF00FFFF;  // IM_COL32 order (A B G R from high to low)
};

enum class DebugCommand : std::uint8_t {
    None,
    Run,
    Pause,
    StepInto,
    StepOver,
    StepOut,
    StepLine,
    StepFrame,
    RunTo,
    SoftReset,
    HardReset,
};

class DebuggerUi {
public:
    DebuggerUi();
    ~DebuggerUi();

    // A new machine (ROM opened): the panels follow it; null detaches.
    void attach(Machine* machine, debug::Debugger* debugger);
    // The machine stopped (pause, step, breakpoint): refresh the "changed since last stop" snapshots and
    // follow the PC.
    void onStop();

    void draw(bool running);
    // Debug menu entries.
    void drawMenu();
    // Opens every panel (Debug > Open All Panels, --debugger).
    void openAll();
    bool anyOpen() const;
    // Window names of all panels, for the dock layout.
    static std::vector<std::string> windowNames();

    DebugCommand takeCommand() {
        const DebugCommand c = command_;
        command_ = DebugCommand::None;
        return c;
    }
    void request(DebugCommand c) { command_ = c; }
    std::uint32_t runToAddress() const { return runTo_; }
    CpuId focus() const { return focus_; }
    void setFocus(CpuId cpu) { focus_ = cpu; }

    // Sprite boxes for the game view (empty unless enabled in the Sprites panel).
    const std::vector<ScreenBox>& overlayBoxes() const { return boxes_; }
    void setDefaultTracePath(std::string path);
    void setSpriteBoxes(bool on) { spriteBoxes_ = on; }

private:
    struct MemoryView;

    void drawRunControl(bool running);
    void drawDisassembly(bool running);
    void drawM68kRegisters(bool running);
    void drawZ80Registers(bool running);
    void drawMemory(int index, bool running);
    void drawBreakpoints(bool running);
    void drawTrace(bool running);
    void drawCallStack(bool running);
    void drawVdpRegisters();
    void drawTiles();
    void drawPlanes();
    void drawSprites();
    void drawPalette();
    void gotoDisassembly(CpuId cpu, std::uint32_t addr);

    Machine* m_ = nullptr;
    debug::Debugger* d_ = nullptr;
    DebugCommand command_ = DebugCommand::None;
    CpuId focus_ = CpuId::M68k;
    std::uint32_t runTo_ = 0;

    bool showDebugger_ = false;
    bool showDisasm_ = false;
    bool show68k_ = false;
    bool showZ80_ = false;
    bool showBreakpoints_ = false;
    bool showTrace_ = false;
    bool showCallStack_ = false;
    bool showVdpRegs_ = false;
    bool showTiles_ = false;
    bool showPlanes_ = false;
    bool showSprites_ = false;
    bool showPalette_ = false;

    // Disassembly.
    debug::KnownStarts known_;
    bool followPc_ = true;
    std::uint32_t disasmTop_ = 0;
    bool disasmTopValid_ = false;
    std::array<char, 16> disasmGoto_{};
    std::uint32_t lastPc_[2]{};

    // Register snapshots at the previous stop (changed-register highlighting).
    m68k::State prev68k_{};
    m68k::State cur68k_{};
    z80::Z80State prevZ80_{};
    z80::Z80State curZ80_{};
    bool haveSnapshot_ = false;

    std::array<std::unique_ptr<MemoryView>, kMemoryViews> memory_;
    std::vector<debug::RomPatch> patches_;

    // Breakpoint editor.
    debug::Breakpoint form_;
    std::array<char, 16> formStart_{};
    std::array<char, 16> formEnd_{};
    std::array<char, 256> formCondition_{};
    std::array<char, 256> formMessage_{};
    std::string formError_;
    int editing_ = -1;  // id of the breakpoint loaded into the form

    // Trace.
    std::array<char, 512> tracePath_{};
    std::array<char, 512> ringPath_{};
    std::array<char, 16> tracePcLo_{};
    std::array<char, 16> tracePcHi_{};
    bool traceFirst_ = false;
    int traceFrames_ = 0;
    int traceMax_ = 0;
    int ringSize_ = 100000;
    std::string traceStatus_;

    // VDP viewers.
    Texture tiles_;
    Texture plane_;
    int tilePalette_ = 0;
    int planeSel_ = 0;  // 0 = A, 1 = B, 2 = window
    bool planeViewport_ = true;
    bool spriteBoxes_ = false;
    std::vector<ScreenBox> boxes_;
};

}  // namespace gen::app
