#pragma once
// VDP (spec 05): command latch, registers, VRAM / CRAM / VSRAM, status register with a small FIFO model,
// HV counter, V-int / H-int, DMA with 68k freeze timing, and the scanline renderer (planes, window, sprites,
// priority, shadow / highlight, interlace mode 2) into an RGBA8888 framebuffer (M03).

#include <array>
#include <cstdint>
#include <vector>

#include "core/hooks.hpp"

namespace gen::state {
class Archive;
}

namespace gen::vdp {

enum class DmaKind : std::uint8_t { MemoryToVdp, Fill, Copy };
enum class DmaTarget : std::uint8_t { Vram, Cram, Vsram };

inline constexpr std::size_t kVramSize = 0x10000;
inline constexpr std::size_t kCramWords = 64;
inline constexpr std::size_t kVsramWords = 40;
inline constexpr int kRegisterCount = 24;
inline constexpr int kMaxSprites = 80;
inline constexpr int kSatCacheBytes = kMaxSprites * 4;  // Y, size and link of every sprite (first 4 bytes)
inline constexpr int kFifoDepth = 4;

// Framebuffer: always 320 pixels per row; 240 rows progressive, 480 in interlace mode 2.
inline constexpr int kFbWidth = 320;
inline constexpr int kFbHeight = 480;

// Bytes per scanline a DMA moves (spec 05 "DMA": approximate, configurable).
struct DmaRates {
    int h40Active = 18;
    int h40Blank = 205;
    int h32Active = 16;
    int h32Blank = 167;
};

struct VdpState {
    std::array<std::uint8_t, kRegisterCount> regs{};
    std::array<std::uint8_t, kVramSize> vram{};
    std::array<std::uint16_t, kCramWords> cram{};
    std::array<std::uint16_t, kVsramWords> vsram{};
    std::array<std::uint8_t, kSatCacheBytes> satCache{};  // internal sprite attribute cache
    std::uint16_t address = 0;                            // 16-bit access address (A15-A0)
    std::uint16_t addressHigh = 0;                        // A15-A14 latched by the last second command word
    std::uint8_t code = 0;                                // CD5-CD0: access target, DMA bit
    bool pending = false;      // first command word latched, waiting for the second
    bool fillPending = false;  // DMA fill armed: the next data-port write starts it
    bool vintPending = false;  // status F flag
    bool hintPending = false;
    bool spriteOverflow = false;
    bool spriteCollision = false;
    bool oddFrame = false;
    bool pal = false;
    bool z80Int = false;     // Z80 /INT, asserted for the first V-blank line
    bool vintArmed = false;  // the first V-blank line has begun; V-int is raised vintDelayMclk() into it
    int hintCounter = 0;
    int line = 0;  // current scanline (0 = first active line)
    std::uint16_t readBuffer = 0;
    int fifoUsed = 0;  // approximate FIFO occupancy (status bits only)
    // 68k -> VDP DMA in progress (the 68k is frozen while it runs).
    bool dmaActive = false;
    std::uint32_t dmaRemaining = 0;  // words still to transfer
    std::uint32_t dmaSource = 0;     // next 68k byte address
    std::uint64_t dmaCredit = 0;     // byte-MCLK products not yet turned into a word
    // Fill / copy run instantly but keep the status DMA busy flag for their approximate duration.
    std::uint32_t dmaBusyBytes = 0;
    bool prevLineDotOverflow = false;  // sprite masking rule
};

// Layer debug toggles (spec 05 "Layer debug toggles"); not part of the emulated state.
struct LayerMask {
    enum class Priority : std::uint8_t { Both, HighOnly, LowOnly };
    bool planeB = true;
    bool planeA = true;
    bool window = true;
    bool sprites = true;
    Priority priority = Priority::Both;
};

// One decoded sprite attribute table entry (inspection API).
struct SpriteInfo {
    int index = 0;
    int x = 0;  // screen coordinates (raw - 128)
    int y = 0;  // screen coordinates (raw - 128, or raw - 256 in interlace mode 2)
    int widthCells = 1;
    int heightCells = 1;
    int link = 0;
    std::uint16_t tile = 0;
    std::uint8_t palette = 0;
    bool priority = false;
    bool hflip = false;
    bool vflip = false;
};

// Per-line values recorded while rendering (inspection API).
struct LineInfo {
    std::uint16_t hscrollA = 0;
    std::uint16_t hscrollB = 0;
    std::uint16_t vscrollA = 0;  // first column
    std::uint16_t vscrollB = 0;
    std::uint16_t cramWrites = 0;  // CRAM writes that happened while this line was being drawn
    std::uint8_t sprites = 0;      // sprites on the line
};

}  // namespace gen::vdp

namespace gen {

// onDma hook payload (spec 01 / 05).
struct DmaEvent {
    vdp::DmaKind kind = vdp::DmaKind::MemoryToVdp;
    vdp::DmaTarget target = vdp::DmaTarget::Vram;
    std::uint32_t source = 0;  // 68k byte address (MemoryToVdp) or VRAM byte address (Copy); 0 for Fill
    std::uint32_t dest = 0;    // byte address in the target memory
    std::uint32_t length = 0;  // words (MemoryToVdp) or bytes (Fill, Copy)
};

}  // namespace gen

namespace gen::vdp {

enum class Intensity : std::uint8_t { Normal, Shadow, Highlight };
// CRAM word (0000 BBB0 GGG0 RRR0) to RGBA8888 (byte order R, G, B, A) at the given intensity.
std::uint32_t cramToRgba(std::uint16_t cram, Intensity intensity = Intensity::Normal);

class Vdp {
public:
    // Reads one word of 68k memory for DMA (ROM / RAM, no side effects).
    using DmaReadFn = std::uint16_t (*)(void* ctx, std::uint32_t addr);

    Vdp();
    void reset(bool pal);
    void setDmaReader(DmaReadFn fn, void* ctx) {
        dmaRead_ = fn;
        dmaCtx_ = ctx;
    }
    void setHooks(const Hooks* hooks) { hooks_ = hooks; }
    void setDmaRates(const DmaRates& rates) { rates_ = rates; }

    // Ports. `lineMclk` is the master-clock position inside the current scanline (0-3419).
    std::uint16_t readData();
    void writeData(std::uint16_t value);
    std::uint16_t readStatus(int lineMclk);
    void writeControl(std::uint16_t value);
    std::uint16_t readHvCounter(int lineMclk) const;

    // Scheduler. beginLine: scanline `line` starts (0 = first active line): V-blank, V-int at the first
    // V-blank line, Z80 /INT. endLine: the line's active display is over: it is rendered, the H-int
    // counter steps, and FIFO / fill / copy time passes.
    void beginLine(int line);
    void endLine(int line);

    // 68k -> VDP DMA: while dmaFreezesCpu(), the scheduler gives the VDP time instead of running the 68k.
    // runDma transfers what fits in `mclk` master clocks and returns the clocks it used (<= mclk).
    bool dmaFreezesCpu() const { return s_.dmaActive; }
    int runDma(int mclk);

    // Interrupt level the VDP drives on the 68k IPL lines (6, 4 or 0).
    int irqLevel() const;
    void acknowledge(int level);

    // V-int timing (M06-00b). A scanline boundary of the scheduler is where the V counter increments and
    // H-int fires (H counter $A5 in H40, $85 in H32). V-int (and the status F flag) comes later, at H counter
    // $00 -> $01 of the first V-blank line: 46 H40 units (~749 MCLK) or 39 H32 units (780 MCLK) after the
    // boundary. So an H-int raised at the end of the last active line is taken before the V-int, as on
    // hardware (Sonic 2's water handler relies on it). The scheduler calls raiseVint() once the 68k reaches
    // that point (plus any Z80 stall it charged to the 68k at the boundary).
    bool vintArmed() const { return s_.vintArmed; }
    int vintDelayMclk() const { return h40() ? 749 : 780; }
    void raiseVint() {
        s_.vintArmed = false;
        s_.vintPending = true;
    }

    int activeHeight() const { return (s_.regs[1] & 0x08) != 0 && s_.pal ? 240 : 224; }
    bool h40() const { return (s_.regs[12] & 0x01) != 0; }
    bool interlace2() const { return (s_.regs[12] & 0x06) == 0x06; }
    bool displayEnabled() const { return (s_.regs[1] & 0x40) != 0; }
    bool inVblank() const { return s_.line >= activeHeight() || !displayEnabled(); }

    // Framebuffer (RGBA8888, byte order R, G, B, A; row stride kFbWidth pixels). frameWidth/Height give
    // the visible part for the current mode: 256 or 320 wide, 224 / 240 (448 / 480 in interlace mode 2).
    const std::uint32_t* framebuffer() const { return fb_.data(); }
    int frameWidth() const { return frameWidth_; }
    int frameHeight() const { return frameHeight_; }
    std::uint64_t frameHash() const;  // XXH3-64 over the visible pixels and the dimensions

    LayerMask& layers() { return layers_; }
    std::vector<SpriteInfo> decodeSprites() const;  // link-list order from sprite 0
    const std::array<LineInfo, kFbHeight / 2>& lineInfo() const { return lineInfo_; }

    VdpState& state() { return s_; }
    const VdpState& state() const { return s_; }
    // After restoring state() from outside (save states, tests): rebuild derived caches.
    void invalidateCaches() { paletteDirty_ = true; }
    // Save states: VdpState plus the visible framebuffer, so a loaded state shows its picture at once.
    void serialize(state::Archive& ar);

private:
    int autoIncrement() const { return s_.regs[15]; }
    std::uint32_t dmaLength() const;
    DmaTarget currentTarget() const;
    void writeVram(std::uint16_t addr, std::uint8_t value);
    void writeTarget(std::uint16_t value);
    void reportWrite(std::uint16_t value) const;  // onVdpWrite for a data-port / DMA write
    void startDma();
    void finishMemoryDma();
    void runFill(std::uint16_t value);
    void runCopy();
    int dmaBytesPerLine() const;
    std::uint8_t vCounter() const;
    std::uint8_t hCounter(int lineMclk) const;

    // Rendering (vdp_render.cpp).
    void renderLine(int line);
    void updatePalette();
    void renderPlane(std::uint8_t* out, bool planeA, int y, int width);
    void renderWindow(std::uint8_t* out, int y, int from, int to);
    void renderSprites(std::uint8_t* out, int y, int width, int line);

    VdpState s_;
    DmaRates rates_;
    DmaReadFn dmaRead_ = nullptr;
    void* dmaCtx_ = nullptr;
    const Hooks* hooks_ = nullptr;
    DmaEvent dmaEvent_;

    // Derived / output state (not hashed into the machine state).
    std::vector<std::uint32_t> fb_;
    int frameWidth_ = 320;
    int frameHeight_ = 224;
    bool paletteDirty_ = true;
    std::array<std::uint32_t, 64 * 3> palette_{};  // normal, shadow, highlight RGBA per CRAM entry
    LayerMask layers_;
    std::array<LineInfo, kFbHeight / 2> lineInfo_{};
    std::uint16_t cramWritesThisLine_ = 0;
};

}  // namespace gen::vdp
