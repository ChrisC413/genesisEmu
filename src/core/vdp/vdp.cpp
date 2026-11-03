#include "core/vdp/vdp.hpp"

#include <algorithm>
#include <bit>
#include <span>

#include "core/state/archive.hpp"
#include "core/state/state_hash.hpp"

namespace gen::vdp {

namespace {

constexpr std::uint16_t kStatusFixedBits = 0x3400;  // upper bits approximate open bus
constexpr std::uint16_t kStatusFifoEmpty = 0x0200;
constexpr std::uint16_t kStatusFifoFull = 0x0100;
constexpr std::uint16_t kStatusVint = 0x0080;
constexpr std::uint16_t kStatusOverflow = 0x0040;
constexpr std::uint16_t kStatusCollision = 0x0020;
constexpr std::uint16_t kStatusOddFrame = 0x0010;
constexpr std::uint16_t kStatusVblank = 0x0008;
constexpr std::uint16_t kStatusHblank = 0x0004;
constexpr std::uint16_t kStatusDmaBusy = 0x0002;
constexpr std::uint16_t kStatusPal = 0x0001;

constexpr int kMclkPerLine = 3420;
constexpr std::uint64_t kWordCost = 2ull * kMclkPerLine;  // byte-MCLK products per transferred word

// Command codes (CD3-CD0).
constexpr std::uint8_t kCodeVramRead = 0x00;
constexpr std::uint8_t kCodeVramWrite = 0x01;
constexpr std::uint8_t kCodeCramWrite = 0x03;
constexpr std::uint8_t kCodeVsramRead = 0x04;
constexpr std::uint8_t kCodeVsramWrite = 0x05;
constexpr std::uint8_t kCodeCramRead = 0x08;
constexpr std::uint8_t kCodeVram8Read = 0x0C;

}  // namespace

Vdp::Vdp() : fb_(static_cast<std::size_t>(kFbWidth) * kFbHeight, 0xFF000000u) {}

void Vdp::reset(bool pal) {
    s_ = VdpState{};
    s_.pal = pal;
    std::fill(fb_.begin(), fb_.end(), 0xFF000000u);
    frameWidth_ = 256;
    frameHeight_ = 224;
    paletteDirty_ = true;
    lineInfo_ = {};
    cramWritesThisLine_ = 0;
}

std::uint32_t Vdp::dmaLength() const {
    const std::uint32_t len = s_.regs[19] | (static_cast<std::uint32_t>(s_.regs[20]) << 8);
    return len == 0 ? 0x10000 : len;
}

DmaTarget Vdp::currentTarget() const {
    switch (s_.code & 0x0F) {
        case kCodeCramWrite:
            return DmaTarget::Cram;
        case kCodeVsramWrite:
            return DmaTarget::Vsram;
        default:
            return DmaTarget::Vram;
    }
}

void Vdp::writeVram(std::uint16_t addr, std::uint8_t value) {
    s_.vram[addr] = value;
    // The VDP keeps its own copy of the first four bytes (Y, size, link) of every sprite table entry;
    // it is updated only by VRAM writes inside the table.
    const std::uint16_t base = static_cast<std::uint16_t>((s_.regs[5] << 9) & (h40() ? 0xFC00 : 0xFE00));
    const auto off = static_cast<std::uint16_t>(addr - base);
    if (off < kMaxSprites * 8 && (off & 4) == 0) {
        s_.satCache[static_cast<std::size_t>((off >> 3) * 4 + (off & 3))] = value;
    }
}

void Vdp::writeTarget(std::uint16_t value) {
    switch (s_.code & 0x0F) {
        case kCodeVramWrite: {  // an odd address swaps the bytes
            const std::uint16_t v =
                (s_.address & 1) != 0 ? static_cast<std::uint16_t>((value << 8) | (value >> 8)) : value;
            const auto a = static_cast<std::uint16_t>(s_.address & 0xFFFE);
            writeVram(a, static_cast<std::uint8_t>(v >> 8));
            writeVram(static_cast<std::uint16_t>(a + 1), static_cast<std::uint8_t>(v));
            break;
        }
        case kCodeCramWrite:  // 9-bit color 0000 BBB0 GGG0 RRR0; 64 words, the address wraps
            s_.cram[(s_.address >> 1) & 0x3F] = static_cast<std::uint16_t>(value & 0x0EEE);
            paletteDirty_ = true;
            ++cramWritesThisLine_;
            break;
        case kCodeVsramWrite: {  // 40 words; addresses 40-63 of the 64-word window are not backed
            const std::size_t i = (s_.address >> 1) & 0x3F;
            if (i < kVsramWords) {
                s_.vsram[i] = static_cast<std::uint16_t>(value & 0x07FF);
            }
            break;
        }
        default:  // read codes or invalid: the write is dropped
            break;
    }
    s_.address = static_cast<std::uint16_t>(s_.address + autoIncrement());
}

std::uint16_t Vdp::readData() {
    s_.pending = false;
    std::uint16_t v = s_.readBuffer;
    switch (s_.code & 0x0F) {
        case kCodeVramRead: {
            const std::size_t a = s_.address & 0xFFFE;
            v = static_cast<std::uint16_t>((s_.vram[a] << 8) | s_.vram[a + 1]);
            break;
        }
        case kCodeCramRead:  // unused bits come from the read buffer
            v = static_cast<std::uint16_t>((s_.readBuffer & ~0x0EEE) | s_.cram[(s_.address >> 1) & 0x3F]);
            break;
        case kCodeVsramRead: {
            const std::size_t i = (s_.address >> 1) & 0x3F;
            v = static_cast<std::uint16_t>((s_.readBuffer & 0xF800) |
                                           (i < kVsramWords ? s_.vsram[i] : s_.vsram[0]));
            break;
        }
        case kCodeVram8Read:  // undocumented 8-bit VRAM read: the byte lands in the low half
            v = static_cast<std::uint16_t>((s_.readBuffer & 0xFF00) | s_.vram[s_.address ^ 1u]);
            break;
        default:
            break;
    }
    s_.readBuffer = v;
    s_.address = static_cast<std::uint16_t>(s_.address + autoIncrement());
    return v;
}

void Vdp::writeData(std::uint16_t value) {
    s_.pending = false;
    if (!inVblank()) {
        s_.fifoUsed = std::min(s_.fifoUsed + 1, kFifoDepth);
    }
    if (s_.fillPending) {
        s_.fillPending = false;
        runFill(value);
        return;
    }
    writeTarget(value);
}

void Vdp::runFill(std::uint16_t value) {
    const std::uint16_t start = s_.address;
    const std::uint32_t len = dmaLength();
    const DmaTarget target = currentTarget();
    writeTarget(value);
    const auto fillByte = static_cast<std::uint8_t>(value >> 8);
    for (std::uint32_t i = 0; i < len; ++i) {
        if (target == DmaTarget::Vram) {
            writeVram(static_cast<std::uint16_t>(s_.address ^ 1u), fillByte);
            s_.address = static_cast<std::uint16_t>(s_.address + autoIncrement());
        } else {
            writeTarget(value);
        }
    }
    s_.regs[19] = 0;
    s_.regs[20] = 0;
    s_.dmaBusyBytes = len;
    if (hooks_ != nullptr && hooks_->onDma != nullptr) {
        hooks_->onDma(hooks_->ctx, DmaEvent{DmaKind::Fill, target, 0, start, len});
    }
}

void Vdp::runCopy() {
    const std::uint32_t len = dmaLength();
    auto src = static_cast<std::uint16_t>(s_.regs[21] | (s_.regs[22] << 8));
    const DmaEvent ev{DmaKind::Copy, DmaTarget::Vram, src, s_.address, len};
    for (std::uint32_t i = 0; i < len; ++i) {
        writeVram(s_.address, s_.vram[src]);
        ++src;
        s_.address = static_cast<std::uint16_t>(s_.address + autoIncrement());
    }
    s_.regs[19] = 0;
    s_.regs[20] = 0;
    s_.regs[21] = static_cast<std::uint8_t>(src);
    s_.regs[22] = static_cast<std::uint8_t>(src >> 8);
    s_.dmaBusyBytes = len * 2;  // a copy needs a read and a write slot per byte
    if (hooks_ != nullptr && hooks_->onDma != nullptr) {
        hooks_->onDma(hooks_->ctx, ev);
    }
}

void Vdp::startDma() {
    const int mode = s_.regs[23] >> 6;
    if (mode == 2) {
        s_.fillPending = true;
        return;
    }
    if (mode == 3) {
        runCopy();
        return;
    }
    // 68k memory -> VRAM / CRAM / VSRAM: runs while the scheduler freezes the 68k (runDma).
    s_.dmaActive = true;
    s_.dmaRemaining = dmaLength();
    s_.dmaSource = ((static_cast<std::uint32_t>(s_.regs[23]) & 0x7F) << 17) |
                   (static_cast<std::uint32_t>(s_.regs[22]) << 9) |
                   (static_cast<std::uint32_t>(s_.regs[21]) << 1);
    s_.dmaCredit = 0;
    if (hooks_ != nullptr && hooks_->onDma != nullptr) {
        hooks_->onDma(hooks_->ctx, DmaEvent{DmaKind::MemoryToVdp, currentTarget(), s_.dmaSource, s_.address,
                                            s_.dmaRemaining});
    }
}

int Vdp::dmaBytesPerLine() const {
    if (h40()) {
        return inVblank() ? rates_.h40Blank : rates_.h40Active;
    }
    return inVblank() ? rates_.h32Blank : rates_.h32Active;
}

int Vdp::runDma(int mclk) {
    if (!s_.dmaActive || mclk <= 0) {
        return 0;
    }
    const auto rate = static_cast<std::uint64_t>(std::max(dmaBytesPerLine(), 1));
    const std::uint64_t before = s_.dmaCredit;
    s_.dmaCredit += rate * static_cast<std::uint64_t>(mclk);
    std::uint64_t words = s_.dmaCredit / kWordCost;
    int used = mclk;
    if (words >= s_.dmaRemaining) {
        words = s_.dmaRemaining;
        const std::uint64_t needed = words * kWordCost - before;
        used = static_cast<int>(
            std::min<std::uint64_t>((needed + rate - 1) / rate, static_cast<std::uint64_t>(mclk)));
        s_.dmaCredit = 0;
    } else {
        s_.dmaCredit -= words * kWordCost;
    }
    for (std::uint64_t i = 0; i < words; ++i) {
        writeTarget(dmaRead_ != nullptr ? dmaRead_(dmaCtx_, s_.dmaSource) : 0);
        // The source wraps inside a 128 KB window (A23-A17 stay fixed).
        s_.dmaSource = (s_.dmaSource & 0xFE0000) | ((s_.dmaSource + 2) & 0x1FFFF);
    }
    s_.dmaRemaining -= static_cast<std::uint32_t>(words);
    // Length and source registers count down / up as the transfer runs.
    s_.regs[19] = static_cast<std::uint8_t>(s_.dmaRemaining);
    s_.regs[20] = static_cast<std::uint8_t>(s_.dmaRemaining >> 8);
    s_.regs[21] = static_cast<std::uint8_t>(s_.dmaSource >> 1);
    s_.regs[22] = static_cast<std::uint8_t>(s_.dmaSource >> 9);
    if (s_.dmaRemaining == 0) {
        s_.dmaActive = false;
    }
    return used;
}

void Vdp::writeControl(std::uint16_t value) {
    if (s_.pending) {
        s_.addressHigh = static_cast<std::uint16_t>((value & 0x03) << 14);
        s_.address = static_cast<std::uint16_t>((s_.address & 0x3FFF) | s_.addressHigh);
        s_.code = static_cast<std::uint8_t>((s_.code & 0x03) | ((value >> 2) & 0x3C));
        s_.pending = false;
        if ((s_.code & 0x20) != 0 && (s_.regs[1] & 0x10) != 0) {
            startDma();
        }
        return;
    }
    if ((value & 0xC000) == 0x8000) {  // register write: 10?R RRRR DDDD DDDD
        const int reg = (value >> 8) & 0x1F;
        if (reg < kRegisterCount) {
            s_.regs[static_cast<std::size_t>(reg)] = static_cast<std::uint8_t>(value);
        }
    } else {
        s_.pending = true;
    }
    // Either way the word also loads A13-A0 and CD1-CD0 (a register write leaves CD1-CD0 = 2, an invalid
    // target, so stray data writes after it are dropped as on hardware).
    s_.code = static_cast<std::uint8_t>((s_.code & 0x3C) | (value >> 14));
    s_.address = static_cast<std::uint16_t>(s_.addressHigh | (value & 0x3FFF));
}

std::uint8_t Vdp::vCounter() const {
    int v = s_.line;
    if (!s_.pal) {
        if (v > 0xEA) {
            v += 0x1FA;  // 0xEB -> 0x1E5
        }
    } else if (v > ((s_.regs[1] & 0x08) != 0 ? 0x10A : 0x102)) {
        v += 0xC7;  // 0x103 -> 0x1CA (V28), 0x10B -> 0x1D2 (V30)
    }
    v &= 0x1FF;
    switch ((s_.regs[12] >> 1) & 0x03) {
        case 1:  // interlace mode 1: bit 0 shows bit 8
            return static_cast<std::uint8_t>((v & 0xFE) | ((v >> 8) & 1));
        case 3:  // interlace mode 2: doubled line number, bit 0 shows bit 8
            return static_cast<std::uint8_t>(((v << 1) & 0xFE) | ((v >> 8) & 1));
        default:
            return static_cast<std::uint8_t>(v);
    }
}

std::uint8_t Vdp::hCounter(int lineMclk) const {
    // Approximation from the master-clock position: 210 counts per line in H40, 171 in H32, with the
    // hardware jump in the sequence (H40 $B6 -> $E4, H32 $93 -> $E9).
    lineMclk = std::clamp(lineMclk, 0, kMclkPerLine - 1);
    if (h40()) {
        const int i = lineMclk * 210 / kMclkPerLine;
        return static_cast<std::uint8_t>(i <= 0xB6 ? i : i + (0xE4 - 0xB7));
    }
    const int i = lineMclk / 20;
    return static_cast<std::uint8_t>(i <= 0x93 ? i : i + (0xE9 - 0x94));
}

std::uint16_t Vdp::readHvCounter(int lineMclk) const {
    return static_cast<std::uint16_t>((vCounter() << 8) | hCounter(lineMclk));
}

std::uint16_t Vdp::readStatus(int lineMclk) {
    s_.pending = false;
    const std::uint8_t h = hCounter(lineMclk);
    const bool hblank = h40() ? (h >= 0xB3 || h < 0x06) : (h >= 0x93 || h < 0x05);
    std::uint16_t v = kStatusFixedBits;
    v |= s_.fifoUsed == 0 ? kStatusFifoEmpty : 0;
    v |= s_.fifoUsed >= kFifoDepth ? kStatusFifoFull : 0;
    v |= s_.vintPending ? kStatusVint : 0;
    v |= s_.spriteOverflow ? kStatusOverflow : 0;
    v |= s_.spriteCollision ? kStatusCollision : 0;
    v |= s_.oddFrame ? kStatusOddFrame : 0;
    v |= inVblank() ? kStatusVblank : 0;
    v |= hblank ? kStatusHblank : 0;
    v |= s_.dmaActive || s_.fillPending || s_.dmaBusyBytes != 0 ? kStatusDmaBusy : 0;
    v |= s_.pal ? kStatusPal : 0;
    s_.spriteOverflow = false;
    s_.spriteCollision = false;
    return v;
}

void Vdp::beginLine(int line) {
    if (s_.vintArmed) {
        raiseVint();  // the scheduler did not get to it within the line (cannot happen with 3420-MCLK lines)
    }
    s_.line = line;
    const int height = activeHeight();
    if (line == 0) {
        if ((s_.regs[12] & 0x02) != 0) {
            s_.oddFrame = !s_.oddFrame;
        } else {
            s_.oddFrame = false;
        }
        frameWidth_ = h40() ? 320 : 256;
        frameHeight_ = interlace2() ? height * 2 : height;
    }
    s_.z80Int = line == height;  // /INT is held for about one line
    if (line == height) {
        s_.vintArmed = true;  // raised vintDelayMclk() into the line (see raiseVint)
    }
}

void Vdp::endLine(int line) {
    const int height = activeHeight();
    if (line < height) {
        renderLine(line);
    }
    // H-int counter: decremented after every active line (and the first V-blank line), reloaded in
    // V-blank. An underflow raises H-int; its handler runs during the next line, so a CRAM change it makes
    // shows from the next rendered line on.
    if (line <= height) {
        if (--s_.hintCounter < 0) {
            s_.hintCounter = s_.regs[10];
            s_.hintPending = true;
        }
    } else {
        s_.hintCounter = s_.regs[10];
    }
    s_.fifoUsed = 0;
    const auto rate = static_cast<std::uint32_t>(dmaBytesPerLine());
    s_.dmaBusyBytes = s_.dmaBusyBytes > rate ? s_.dmaBusyBytes - rate : 0;
}

int Vdp::irqLevel() const {
    if (s_.vintPending && (s_.regs[1] & 0x20) != 0) {
        return 6;
    }
    if (s_.hintPending && (s_.regs[0] & 0x10) != 0) {
        return 4;
    }
    return 0;
}

void Vdp::acknowledge(int level) {
    if (level == 6) {
        s_.vintPending = false;
    } else if (level == 4) {
        s_.hintPending = false;
    }
}

std::uint64_t Vdp::frameHash() const {
    static_assert(std::endian::native == std::endian::little, "framebuffer bytes are hashed as stored");
    state::StateHasher h;
    h.tag("FRAM");
    h.i32(frameWidth_);
    h.i32(frameHeight_);
    for (int y = 0; y < frameHeight_; ++y) {
        const auto* row =
            reinterpret_cast<const std::uint8_t*>(fb_.data() + static_cast<std::size_t>(y) * kFbWidth);
        h.bytes(std::span<const std::uint8_t>(row, static_cast<std::size_t>(frameWidth_) * 4));
    }
    return h.digest();
}

}  // namespace gen::vdp

namespace gen::vdp {

void Vdp::serialize(state::Archive& ar) {
    VdpState& v = s_;
    ar.bytes(v.regs);
    ar.bytes(v.vram);
    ar.u16s(v.cram);
    ar.u16s(v.vsram);
    ar.bytes(v.satCache);
    ar.u16(v.address);
    ar.u16(v.addressHigh);
    ar.u8(v.code);
    for (bool* b : {&v.pending, &v.fillPending, &v.vintPending, &v.hintPending, &v.spriteOverflow,
                    &v.spriteCollision, &v.oddFrame, &v.pal, &v.z80Int, &v.vintArmed}) {
        ar.boolean(*b);
    }
    ar.i32(v.hintCounter);
    ar.i32(v.line);
    ar.u16(v.readBuffer);
    ar.i32(v.fifoUsed);
    ar.boolean(v.dmaActive);
    ar.u32(v.dmaRemaining);
    ar.u32(v.dmaSource);
    ar.u64(v.dmaCredit);
    ar.u32(v.dmaBusyBytes);
    ar.boolean(v.prevLineDotOverflow);
    // Output side: frame size, the visible pixels and the per-line CRAM write count in progress.
    ar.i32(frameWidth_);
    ar.i32(frameHeight_);
    if (ar.loading() &&
        (frameWidth_ <= 0 || frameWidth_ > kFbWidth || frameHeight_ <= 0 || frameHeight_ > kFbHeight)) {
        ar.fail("bad frame size");
        frameWidth_ = 320;
        frameHeight_ = 224;
        return;
    }
    for (int y = 0; y < frameHeight_; ++y) {
        auto* row = reinterpret_cast<std::uint8_t*>(fb_.data() + static_cast<std::size_t>(y) * kFbWidth);
        ar.bytes(std::span<std::uint8_t>(row, static_cast<std::size_t>(frameWidth_) * 4));
    }
    ar.u16(cramWritesThisLine_);
    if (ar.loading()) {
        paletteDirty_ = true;
    }
}

}  // namespace gen::vdp
