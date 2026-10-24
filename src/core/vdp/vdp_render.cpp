// Scanline renderer (spec 05 "Rendering", M03-04..M03-09). Each layer is drawn into a line buffer of
// "pixel codes" (bit 7 priority, bits 5-4 palette, bits 3-0 color; color 0 is transparent but keeps the
// priority bit, which shadow / highlight needs), then the layers are composed and looked up in CRAM.

#include <algorithm>
#include <array>

#include "core/vdp/vdp.hpp"

namespace gen::vdp {

namespace {

constexpr std::uint8_t kPrio = 0x80;
constexpr std::uint8_t kColorMask = 0x3F;

constexpr bool opaque(std::uint8_t p) {
    return (p & 0x0F) != 0;
}

// Measured-style 15-step DAC ramp: normal intensity uses the even steps, shadow 0-7, highlight 7-14.
constexpr std::array<std::uint8_t, 15> kLevels = {0, 29, 52, 70, 87, 101, 116, 130, 144, 158, 172, 187, 206, 228, 255};

constexpr std::uint32_t rgba(int r, int g, int b) {
    return static_cast<std::uint32_t>(kLevels[static_cast<std::size_t>(r)]) |
           (static_cast<std::uint32_t>(kLevels[static_cast<std::size_t>(g)]) << 8) |
           (static_cast<std::uint32_t>(kLevels[static_cast<std::size_t>(b)]) << 16) | 0xFF000000u;
}

constexpr std::array<int, 4> kPlaneCells = {32, 64, 32, 128};  // reg 16 size codes; 2 is prohibited

// One tile row: 8 pixels, 4 bits each, leftmost pixel in the high nibble of the first byte.
inline int tilePixel(const std::uint8_t* row, int x) {
    const std::uint8_t b = row[x >> 1];
    return (x & 1) != 0 ? (b & 0x0F) : (b >> 4);
}

}  // namespace

void Vdp::updatePalette() {
    for (std::size_t i = 0; i < kCramWords; ++i) {
        const int c = s_.cram[i];
        const int r = (c >> 1) & 7;
        const int g = (c >> 5) & 7;
        const int b = (c >> 9) & 7;
        palette_[i] = rgba(r * 2, g * 2, b * 2);
        palette_[64 + i] = rgba(r, g, b);
        palette_[128 + i] = rgba(r + 7, g + 7, b + 7);
    }
    paletteDirty_ = false;
}

void Vdp::renderPlane(std::uint8_t* out, bool planeA, int y, int width, int /*windowFrom*/, int /*windowTo*/) {
    const bool im2 = interlace2();
    const int tileH = im2 ? 16 : 8;
    const int tileBytes = im2 ? 64 : 32;
    const int wCells = kPlaneCells[s_.regs[16] & 3];
    const int hCells = kPlaneCells[(s_.regs[16] >> 4) & 3];
    const int wMask = wCells * 8 - 1;
    const int hMask = hCells * tileH - 1;
    const std::uint16_t ntBase = planeA ? static_cast<std::uint16_t>((s_.regs[2] & 0x38) << 10)
                                        : static_cast<std::uint16_t>((s_.regs[4] & 0x07) << 13);

    // Horizontal scroll: full screen, per 8-line cell, per line (mode 1 repeats the first 8 entries).
    const int screenLine = s_.line;
    int hsLine = 0;
    switch (s_.regs[11] & 3) {
        case 1:
            hsLine = screenLine & 7;
            break;
        case 2:
            hsLine = screenLine & ~7;
            break;
        case 3:
            hsLine = screenLine;
            break;
        default:
            break;
    }
    const auto hsAddr = static_cast<std::uint16_t>(((s_.regs[13] & 0x3F) << 10) + hsLine * 4 + (planeA ? 0 : 2));
    const int hs = ((s_.vram[hsAddr] << 8) | s_.vram[static_cast<std::uint16_t>(hsAddr + 1)]) & 0x3FF;
    const bool columnScroll = (s_.regs[11] & 0x04) != 0;
    const int vsMask = im2 ? 0x7FF : 0x3FF;
    const int fine = hs & 15;
    const int plane = planeA ? 0 : 1;
    if (planeA) {
        lineInfo_[static_cast<std::size_t>(screenLine)].hscrollA = static_cast<std::uint16_t>(hs);
        lineInfo_[static_cast<std::size_t>(screenLine)].vscrollA = s_.vsram[0];
    } else {
        lineInfo_[static_cast<std::size_t>(screenLine)].hscrollB = static_cast<std::uint16_t>(hs);
        lineInfo_[static_cast<std::size_t>(screenLine)].vscrollB = s_.vsram[1];
    }

    int x = 0;
    while (x < width) {
        int vs = 0;
        int segEnd = width;
        if (columnScroll) {
            // 2-cell columns follow the fine-scrolled fetch grid; the partly visible column left of the
            // first one has no VSRAM entry of its own (approximated as entries 38 and 39 ANDed).
            const int n = ((x - fine + 16) >> 4) - 1;
            vs = n < 0 ? (s_.vsram[38] & s_.vsram[39]) : s_.vsram[static_cast<std::size_t>(std::min(n, 19) * 2 + plane)];
            segEnd = std::min(width, (n + 1) * 16 + fine);
        } else {
            vs = s_.vsram[static_cast<std::size_t>(plane)];
        }
        const int py = (y + (vs & vsMask)) & hMask;
        const int px = (x - hs) & wMask;
        const int fineX = px & 7;
        const int count = std::min({8 - fineX, segEnd - x, width - x});
        const int entryOff = (((py / tileH) * wCells + (px >> 3)) * 2) & 0x1FFF;
        const auto ea = static_cast<std::uint16_t>(ntBase + entryOff);
        const int entry = (s_.vram[ea] << 8) | s_.vram[static_cast<std::uint16_t>(ea + 1)];
        int row = py & (tileH - 1);
        if ((entry & 0x1000) != 0) {
            row = tileH - 1 - row;
        }
        const auto rowAddr = static_cast<std::uint16_t>((entry & 0x7FF) * tileBytes + row * 4);
        const std::uint8_t* data = &s_.vram[rowAddr & 0xFFFC];
        const bool hflip = (entry & 0x0800) != 0;
        const auto attr = static_cast<std::uint8_t>(((entry >> 8) & 0x80) | ((entry >> 9) & 0x30));
        for (int i = 0; i < count; ++i) {
            const int c = fineX + i;
            out[x + i] = static_cast<std::uint8_t>(attr | tilePixel(data, hflip ? 7 - c : c));
        }
        x += count;
    }
}

void Vdp::renderWindow(std::uint8_t* out, int y, int from, int to) {
    const bool im2 = interlace2();
    const int tileH = im2 ? 16 : 8;
    const int tileBytes = im2 ? 64 : 32;
    const int wCells = h40() ? 64 : 32;
    const auto base = static_cast<std::uint16_t>((s_.regs[3] & (h40() ? 0x3C : 0x3E)) << 10);
    const int rowCell = y / tileH;
    for (int x = from; x < to; x += 8) {
        const int col = x >> 3;
        const auto ea = static_cast<std::uint16_t>(base + (((rowCell * wCells + col) * 2) & 0x1FFF));
        const int entry = (s_.vram[ea] << 8) | s_.vram[static_cast<std::uint16_t>(ea + 1)];
        int row = y & (tileH - 1);
        if ((entry & 0x1000) != 0) {
            row = tileH - 1 - row;
        }
        const auto rowAddr = static_cast<std::uint16_t>((entry & 0x7FF) * tileBytes + row * 4);
        const std::uint8_t* data = &s_.vram[rowAddr & 0xFFFC];
        const bool hflip = (entry & 0x0800) != 0;
        const auto attr = static_cast<std::uint8_t>(((entry >> 8) & 0x80) | ((entry >> 9) & 0x30));
        for (int i = 0; i < 8 && x + i < to; ++i) {
            out[x + i] = static_cast<std::uint8_t>(attr | tilePixel(data, hflip ? 7 - i : i));
        }
    }
}

void Vdp::renderSprites(std::uint8_t* out, int y, int width, int line) {
    const bool im2 = interlace2();
    const int tileH = im2 ? 16 : 8;
    const int tileBytes = im2 ? 64 : 32;
    const int maxTotal = h40() ? 80 : 64;
    const int maxPerLine = h40() ? 20 : 16;
    const int dotBudget = h40() ? 320 : 256;
    const int yMask = im2 ? 0x3FF : 0x1FF;
    const int yOffset = im2 ? 256 : 128;
    const auto satBase = static_cast<std::uint16_t>((s_.regs[5] << 9) & (h40() ? 0xFC00 : 0xFE00));

    // Evaluation: follow the link list, collect the sprites on this line.
    std::array<int, 20> found{};
    int count = 0;
    int idx = 0;
    for (int visited = 0; visited < maxTotal; ++visited) {
        const std::uint8_t* c = &s_.satCache[static_cast<std::size_t>(idx * 4)];
        const int sy = (((c[0] << 8) | c[1]) & yMask) - yOffset;
        const int heightPx = ((c[2] & 3) + 1) * tileH;
        if (y >= sy && y < sy + heightPx) {
            if (count >= maxPerLine) {
                s_.spriteOverflow = true;
                break;
            }
            found[static_cast<std::size_t>(count++)] = idx;
        }
        const int link = c[3] & 0x7F;
        if (link == 0 || link >= maxTotal) {
            break;
        }
        idx = link;
    }
    lineInfo_[static_cast<std::size_t>(line)].sprites = static_cast<std::uint8_t>(count);

    // Drawing: earlier sprites win; x = 0 masks the rest of the line once a sprite with x != 0 was seen
    // (or the previous line ran out of sprite dots).
    int dots = 0;
    bool masked = false;
    bool seenNonZero = false;
    bool dotOverflow = false;
    for (int n = 0; n < count && !dotOverflow; ++n) {
        const int i = found[static_cast<std::size_t>(n)];
        const std::uint8_t* c = &s_.satCache[static_cast<std::size_t>(i * 4)];
        const auto ea = static_cast<std::uint16_t>(satBase + i * 8 + 4);
        const int name = (s_.vram[ea] << 8) | s_.vram[static_cast<std::uint16_t>(ea + 1)];
        const int rawX = ((s_.vram[static_cast<std::uint16_t>(ea + 2)] << 8) | s_.vram[static_cast<std::uint16_t>(ea + 3)]) & 0x1FF;
        if (rawX == 0) {
            masked = masked || seenNonZero || s_.prevLineDotOverflow;
        } else {
            seenNonZero = true;
        }
        const int wCells = ((c[2] >> 2) & 3) + 1;
        const int hCells = (c[2] & 3) + 1;
        const int sy = (((c[0] << 8) | c[1]) & yMask) - yOffset;
        int r = y - sy;
        if ((name & 0x1000) != 0) {
            r = hCells * tileH - 1 - r;
        }
        const bool hflip = (name & 0x0800) != 0;
        const auto attr = static_cast<std::uint8_t>(((name >> 8) & 0x80) | ((name >> 9) & 0x30));
        for (int cx = 0; cx < wCells; ++cx) {
            if (dots >= dotBudget) {
                dotOverflow = true;
                break;
            }
            dots += 8;
            if (masked) {
                continue;
            }
            const int cellCol = hflip ? wCells - 1 - cx : cx;
            const int tile = ((name & 0x7FF) + cellCol * hCells + r / tileH) & 0x7FF;
            const auto rowAddr = static_cast<std::uint16_t>(tile * tileBytes + (r & (tileH - 1)) * 4);
            const std::uint8_t* data = &s_.vram[rowAddr & 0xFFFC];
            const int x0 = rawX - 128 + cx * 8;
            for (int p = 0; p < 8; ++p) {
                const int sx = x0 + p;
                if (sx < 0 || sx >= width) {
                    continue;
                }
                const int pix = tilePixel(data, hflip ? 7 - p : p);
                if (pix == 0) {
                    continue;
                }
                if (opaque(out[sx])) {
                    s_.spriteCollision = true;
                } else {
                    out[sx] = static_cast<std::uint8_t>(attr | pix);
                }
            }
        }
    }
    if (dotOverflow) {
        s_.spriteOverflow = true;
    }
    s_.prevLineDotOverflow = dotOverflow;
}

void Vdp::renderLine(int line) {
    if (line >= kFbHeight / 2) {
        return;
    }
    if (paletteDirty_) {
        updatePalette();
    }
    const bool im2 = interlace2();
    const int field = im2 && s_.oddFrame ? 1 : 0;
    const int width = h40() ? 320 : 256;
    const int outRow = im2 ? line * 2 + field : line;
    std::uint32_t* dst = fb_.data() + static_cast<std::size_t>(outRow) * kFbWidth;
    LineInfo& info = lineInfo_[static_cast<std::size_t>(line)];
    info.cramWrites = cramWritesThisLine_;
    cramWritesThisLine_ = 0;
    const std::uint8_t backdrop = s_.regs[7] & kColorMask;

    if (!displayEnabled()) {
        std::fill(dst, dst + width, palette_[backdrop]);
        s_.prevLineDotOverflow = false;
        return;
    }

    const int y = im2 ? line * 2 + field : line;
    std::array<std::uint8_t, kFbWidth> bufB{};
    std::array<std::uint8_t, kFbWidth> bufA{};
    std::array<std::uint8_t, kFbWidth> bufS{};
    if (layers_.planeB) {
        renderPlane(bufB.data(), false, y, width, 0, 0);
    }

    // Window region on this line: whole line when the vertical condition holds, else the horizontal one.
    const int vp = (s_.regs[18] & 0x1F) * 8;
    const bool vWin = (s_.regs[18] & 0x80) != 0 ? line >= vp : line < vp;
    int winFrom = 0;
    int winTo = 0;
    if (vWin) {
        winTo = width;
    } else {
        const int hp = std::min((s_.regs[17] & 0x1F) * 16, width);
        if ((s_.regs[17] & 0x80) != 0) {
            winFrom = hp;
            winTo = width;
        } else {
            winTo = hp;
        }
    }
    if (layers_.planeA && (winFrom > 0 || winTo < width)) {
        renderPlane(bufA.data(), true, y, width, winFrom, winTo);
    }
    if (winTo > winFrom) {
        if (layers_.window) {
            renderWindow(bufA.data(), y, winFrom, winTo);
        } else {
            std::fill(bufA.begin() + winFrom, bufA.begin() + winTo, std::uint8_t{0});
        }
    }
    renderSprites(bufS.data(), y, width, line);
    if (!layers_.sprites) {
        bufS.fill(0);
    }
    if (layers_.priority != LayerMask::Priority::Both) {
        const bool keepHigh = layers_.priority == LayerMask::Priority::HighOnly;
        for (auto* buf : {&bufB, &bufA, &bufS}) {
            for (int x = 0; x < width; ++x) {
                std::uint8_t& p = (*buf)[static_cast<std::size_t>(x)];
                if (((p & kPrio) != 0) != keepHigh) {
                    p = 0;
                }
            }
        }
    }

    const bool shadowHighlight = (s_.regs[12] & 0x08) != 0;
    for (int x = 0; x < width; ++x) {
        const std::uint8_t b = bufB[static_cast<std::size_t>(x)];
        const std::uint8_t a = bufA[static_cast<std::size_t>(x)];
        const std::uint8_t s = bufS[static_cast<std::size_t>(x)];
        // Plane / backdrop pixel beneath the sprite layer.
        std::uint8_t plane = backdrop;
        bool planeHigh = false;
        if (opaque(a) && (a & kPrio) != 0) {
            plane = a;
            planeHigh = true;
        } else if (opaque(b) && (b & kPrio) != 0) {
            plane = b;
            planeHigh = true;
        } else if (opaque(a)) {
            plane = a;
        } else if (opaque(b)) {
            plane = b;
        }
        const bool spriteOnTop = opaque(s) && ((s & kPrio) != 0 || !planeHigh);
        if (!shadowHighlight) {
            dst[x] = palette_[(spriteOnTop ? s : plane) & kColorMask];
            continue;
        }
        // Shadow / highlight: low-priority planes shade the pixel; palette 3 colors 14 / 15 in sprites are
        // highlight / shadow operators on what lies beneath; high-priority sprites are never shaded and
        // color 14 of palettes 0-2 is always normal intensity.
        const bool planesLow = ((a | b) & kPrio) == 0;
        int level = planesLow ? 1 : 0;  // 0 normal, 1 shadow, 2 highlight
        std::uint8_t color = plane;
        if (spriteOnTop) {
            const int sc = s & kColorMask;
            if (sc == 0x3E) {
                level = level == 1 ? 0 : 2;
            } else if (sc == 0x3F) {
                level = 1;
            } else {
                color = s;
                if ((s & kPrio) != 0 || (sc & 0x0F) == 0x0E) {
                    level = 0;
                }
            }
        }
        dst[x] = palette_[static_cast<std::size_t>((level == 1 ? 64 : level == 2 ? 128 : 0) + (color & kColorMask))];
    }
    if ((s_.regs[0] & 0x20) != 0) {
        std::fill(dst, dst + 8, palette_[backdrop]);
    }
}

}  // namespace gen::vdp
