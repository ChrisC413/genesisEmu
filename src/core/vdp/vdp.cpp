#include "core/vdp/vdp.hpp"

namespace gen::vdp {

namespace {

constexpr std::uint16_t kStatusFixedBits = 0x3400;  // upper bits approximate open bus
constexpr std::uint16_t kStatusFifoEmpty = 0x0200;
constexpr std::uint16_t kStatusVint = 0x0080;
constexpr std::uint16_t kStatusOverflow = 0x0040;
constexpr std::uint16_t kStatusCollision = 0x0020;
constexpr std::uint16_t kStatusOddFrame = 0x0010;
constexpr std::uint16_t kStatusVblank = 0x0008;
constexpr std::uint16_t kStatusHblank = 0x0004;
constexpr std::uint16_t kStatusPal = 0x0001;

constexpr int kMclkPerLine = 3420;

}  // namespace

void Vdp::reset(bool pal) {
    s_ = VdpState{};
    s_.pal = pal;
}

std::uint32_t Vdp::dmaLength() const {
    const std::uint32_t len = s_.regs[19] | (static_cast<std::uint32_t>(s_.regs[20]) << 8);
    return len == 0 ? 0x10000 : len;
}

void Vdp::writeTarget(std::uint16_t value) {
    switch (s_.code & 0x0F) {
        case 0x01: {  // VRAM write; an odd address swaps the bytes
            const std::uint16_t v =
                (s_.address & 1) != 0 ? static_cast<std::uint16_t>((value << 8) | (value >> 8)) : value;
            const std::size_t a = s_.address & 0xFFFE;
            s_.vram[a] = static_cast<std::uint8_t>(v >> 8);
            s_.vram[a + 1] = static_cast<std::uint8_t>(v);
            break;
        }
        case 0x03:  // CRAM write: 9-bit color 0000 BBB0 GGG0 RRR0
            s_.cram[(s_.address >> 1) & 0x3F] = static_cast<std::uint16_t>(value & 0x0EEE);
            break;
        case 0x05: {  // VSRAM write
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
    std::uint16_t v = 0;
    switch (s_.code & 0x0F) {
        case 0x00: {
            const std::size_t a = s_.address & 0xFFFE;
            v = static_cast<std::uint16_t>((s_.vram[a] << 8) | s_.vram[a + 1]);
            break;
        }
        case 0x08:
            v = s_.cram[(s_.address >> 1) & 0x3F];
            break;
        case 0x04: {
            const std::size_t i = (s_.address >> 1) & 0x3F;
            v = i < kVsramWords ? s_.vsram[i] : 0;
            break;
        }
        default:
            break;
    }
    s_.address = static_cast<std::uint16_t>(s_.address + autoIncrement());
    return v;
}

void Vdp::writeData(std::uint16_t value) {
    s_.pending = false;
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
    writeTarget(value);
    const auto fillByte = static_cast<std::uint8_t>(value >> 8);
    for (std::uint32_t i = 0; i < len; ++i) {
        if ((s_.code & 0x0F) == 0x01) {
            s_.vram[s_.address ^ 1u] = fillByte;
        } else {
            writeTarget(value);
            continue;
        }
        s_.address = static_cast<std::uint16_t>(s_.address + autoIncrement());
    }
    s_.regs[19] = 0;
    s_.regs[20] = 0;
    if (hooks_ != nullptr && hooks_->onDma != nullptr) {
        const DmaTarget target = (s_.code & 0x0F) == 0x03   ? DmaTarget::Cram
                                 : (s_.code & 0x0F) == 0x05 ? DmaTarget::Vsram
                                                            : DmaTarget::Vram;
        hooks_->onDma(hooks_->ctx, DmaEvent{DmaKind::Fill, target, 0, start, len});
    }
}

void Vdp::startDma() {
    const int mode = s_.regs[23] >> 6;
    const std::uint32_t len = dmaLength();
    if (mode == 2) {
        s_.fillPending = true;
        return;
    }
    DmaEvent ev;
    ev.dest = s_.address;
    ev.length = len;
    if (mode == 3) {  // VRAM copy
        std::uint16_t src = static_cast<std::uint16_t>(s_.regs[21] | (s_.regs[22] << 8));
        ev.kind = DmaKind::Copy;
        ev.source = src;
        for (std::uint32_t i = 0; i < len; ++i) {
            s_.vram[s_.address] = s_.vram[src];
            ++src;
            s_.address = static_cast<std::uint16_t>(s_.address + autoIncrement());
        }
        s_.regs[21] = static_cast<std::uint8_t>(src);
        s_.regs[22] = static_cast<std::uint8_t>(src >> 8);
    } else {  // 68k memory to VRAM / CRAM / VSRAM
        std::uint32_t src = ((static_cast<std::uint32_t>(s_.regs[23]) & 0x7F) << 17) |
                            (static_cast<std::uint32_t>(s_.regs[22]) << 9) |
                            (static_cast<std::uint32_t>(s_.regs[21]) << 1);
        ev.kind = DmaKind::MemoryToVdp;
        ev.source = src;
        ev.target = (s_.code & 0x0F) == 0x03   ? DmaTarget::Cram
                    : (s_.code & 0x0F) == 0x05 ? DmaTarget::Vsram
                                               : DmaTarget::Vram;
        for (std::uint32_t i = 0; i < len; ++i) {
            writeTarget(dmaRead_ != nullptr ? dmaRead_(dmaCtx_, src) : 0);
            src = (src & 0xFE0000) | ((src + 2) & 0x1FFFF);  // the source wraps inside a 128 KB window
        }
        s_.regs[21] = static_cast<std::uint8_t>(src >> 1);
        s_.regs[22] = static_cast<std::uint8_t>(src >> 9);
    }
    s_.regs[19] = 0;
    s_.regs[20] = 0;
    if (hooks_ != nullptr && hooks_->onDma != nullptr) {
        hooks_->onDma(hooks_->ctx, ev);
    }
}

void Vdp::writeControl(std::uint16_t value) {
    if (s_.pending) {
        s_.address = static_cast<std::uint16_t>((s_.address & 0x3FFF) | ((value & 0x03) << 14));
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
        return;
    }
    s_.code = static_cast<std::uint8_t>((s_.code & 0x3C) | (value >> 14));
    s_.address = static_cast<std::uint16_t>((s_.address & 0xC000) | (value & 0x3FFF));
    s_.pending = true;
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
    if (lineMclk < 0) {
        lineMclk = 0;
    } else if (lineMclk >= kMclkPerLine) {
        lineMclk = kMclkPerLine - 1;
    }
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
    std::uint16_t v = kStatusFixedBits | kStatusFifoEmpty;
    v |= s_.vintPending ? kStatusVint : 0;
    v |= s_.spriteOverflow ? kStatusOverflow : 0;
    v |= s_.spriteCollision ? kStatusCollision : 0;
    v |= s_.oddFrame ? kStatusOddFrame : 0;
    v |= inVblank() ? kStatusVblank : 0;
    v |= hblank ? kStatusHblank : 0;
    v |= s_.pal ? kStatusPal : 0;
    s_.spriteOverflow = false;
    s_.spriteCollision = false;
    return v;
}

void Vdp::beginLine(int line) {
    s_.line = line;
    if (line == 0 && (s_.regs[12] & 0x02) != 0) {
        s_.oddFrame = !s_.oddFrame;
    }
    const int height = activeHeight();
    if (line <= height) {
        if (--s_.hintCounter < 0) {
            s_.hintCounter = s_.regs[10];
            s_.hintPending = true;
        }
    } else {
        s_.hintCounter = s_.regs[10];
    }
    if (line == height) {
        s_.vintPending = true;
    }
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

}  // namespace gen::vdp
