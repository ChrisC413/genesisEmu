// VDP scanline renderer on synthetic VRAM: planes, scrolling, window, sprites, priority, shadow / highlight,
// interlace mode 2 (stories M03-04..M03-09; spec 05 "Rendering").

#include <cstdint>
#include <initializer_list>

#include <catch2/catch_test_macros.hpp>

#include "core/vdp/vdp.hpp"

using namespace gen::vdp;

namespace {

constexpr std::uint16_t kPlaneA = 0xC000;
constexpr std::uint16_t kPlaneB = 0xE000;
constexpr std::uint16_t kWindow = 0xB000;
constexpr std::uint16_t kSat = 0xF000;
constexpr std::uint16_t kHscroll = 0xFC00;

void reg(Vdp& v, int r, int value) {
    v.writeControl(static_cast<std::uint16_t>(0x8000 | (r << 8) | value));
}

void command(Vdp& v, std::uint8_t code, std::uint16_t addr) {
    v.writeControl(static_cast<std::uint16_t>(((code & 0x03) << 14) | (addr & 0x3FFF)));
    v.writeControl(static_cast<std::uint16_t>(((code & 0x3C) << 2) | (addr >> 14)));
}

void vram(Vdp& v, std::uint16_t addr, std::initializer_list<std::uint16_t> words) {
    command(v, 0x01, addr);
    for (const auto w : words) {
        v.writeData(w);
    }
}

void cram(Vdp& v, int index, std::uint16_t color) {
    command(v, 0x03, static_cast<std::uint16_t>(index * 2));
    v.writeData(color);
}

void vsram(Vdp& v, int index, std::uint16_t value) {
    command(v, 0x05, static_cast<std::uint16_t>(index * 2));
    v.writeData(value);
}

// Tile `index` filled with color `c` (8x8, or 8x16 in interlace mode 2 when `rows` = 16).
void solidTile(Vdp& v, int index, int c, int rows = 8) {
    const auto w = static_cast<std::uint16_t>(0x1111 * c);
    command(v, 0x01, static_cast<std::uint16_t>(index * rows * 4));
    for (int i = 0; i < rows * 2; ++i) {
        v.writeData(w);
    }
}

// A plane name table entry.
std::uint16_t entry(int tile, int palette = 0, bool prio = false, bool hflip = false, bool vflip = false) {
    return static_cast<std::uint16_t>((prio ? 0x8000 : 0) | (palette << 13) | (vflip ? 0x1000 : 0) |
                                      (hflip ? 0x0800 : 0) | tile);
}

void setCell(Vdp& v, std::uint16_t base, int col, int row, std::uint16_t e, int widthCells = 64) {
    vram(v, static_cast<std::uint16_t>(base + (row * widthCells + col) * 2), {e});
}

// Sprite table entry `i` (raw coordinates: screen + 128).
void sprite(Vdp& v, int i, int x, int y, int w, int h, int link, std::uint16_t name) {
    vram(v, static_cast<std::uint16_t>(kSat + i * 8),
         {static_cast<std::uint16_t>(y + 128),
          static_cast<std::uint16_t>(((w - 1) << 10) | ((h - 1) << 8) | link), name,
          static_cast<std::uint16_t>(x + 128)});
}

Vdp makeVdp() {
    Vdp v;
    v.reset(false);
    reg(v, 15, 2);
    reg(v, 1, 0x44);   // display on, V28
    reg(v, 12, 0x81);  // H40
    reg(v, 2, kPlaneA >> 10);
    reg(v, 3, kWindow >> 10);
    reg(v, 4, kPlaneB >> 13);
    reg(v, 5, kSat >> 9);
    reg(v, 13, kHscroll >> 10);
    reg(v, 16, 0x01);  // 64x32 planes
    // Palette 0: color n = a distinct shade; palette 1 likewise, other hues.
    for (int i = 1; i < 16; ++i) {
        cram(v, i, static_cast<std::uint16_t>((i & 7) << 1));
        cram(v, 16 + i, static_cast<std::uint16_t>((i & 7) << 5));
        cram(v, 32 + i, static_cast<std::uint16_t>((i & 7) << 9));
        cram(v, 48 + i, static_cast<std::uint16_t>(((i & 7) << 9) | ((i & 7) << 1)));
    }
    cram(v, 0, 0x0222);
    return v;
}

void frame(Vdp& v) {
    for (int line = 0; line < 262; ++line) {
        v.beginLine(line);
        v.endLine(line);
    }
}

std::uint32_t px(const Vdp& v, int x, int y) {
    return v.framebuffer()[y * kFbWidth + x];
}

std::uint32_t color(const Vdp& v, int index, Intensity i = Intensity::Normal) {
    return cramToRgba(v.state().cram[static_cast<std::size_t>(index)], i);
}

}  // namespace

TEST_CASE("Backdrop, display disable and left-column blanking", "[vdp][render]") {
    Vdp v = makeVdp();
    reg(v, 7, 0x05);
    frame(v);
    CHECK(v.frameWidth() == 320);
    CHECK(v.frameHeight() == 224);
    CHECK(px(v, 0, 0) == color(v, 5));
    CHECK(px(v, 319, 223) == color(v, 5));

    solidTile(v, 1, 3);
    for (int col = 0; col < 40; ++col) {
        setCell(v, kPlaneB, col, 0, entry(1));
    }
    frame(v);
    CHECK(px(v, 0, 0) == color(v, 3));
    reg(v, 0, 0x24);  // blank the leftmost 8 pixels
    frame(v);
    CHECK(px(v, 7, 0) == color(v, 5));
    CHECK(px(v, 8, 0) == color(v, 3));
    reg(v, 1, 0x04);  // display off: backdrop everywhere
    frame(v);
    CHECK(px(v, 100, 0) == color(v, 5));

    reg(v, 12, 0x00);  // H32
    reg(v, 1, 0x44);
    frame(v);
    CHECK(v.frameWidth() == 256);
}

TEST_CASE("Tile decode: pixel order, palettes, horizontal and vertical flip", "[vdp][render]") {
    Vdp v = makeVdp();
    // Tile 2: row 0 = colors 1..8, rows 1-6 = 9, row 7 = 10.
    vram(v, 2 * 32, {0x1234, 0x5678});
    for (int r = 1; r < 7; ++r) {
        vram(v, static_cast<std::uint16_t>(2 * 32 + r * 4), {0x9999, 0x9999});
    }
    vram(v, 2 * 32 + 28, {0xAAAA, 0xAAAA});
    setCell(v, kPlaneA, 0, 0, entry(2, 1));
    setCell(v, kPlaneA, 1, 0, entry(2, 1, false, true));
    setCell(v, kPlaneA, 2, 0, entry(2, 2, false, false, true));
    frame(v);
    CHECK(px(v, 0, 0) == color(v, 17));
    CHECK(px(v, 7, 0) == color(v, 24));
    CHECK(px(v, 8, 0) == color(v, 24));
    CHECK(px(v, 15, 0) == color(v, 17));
    CHECK(px(v, 16, 0) == color(v, 32 + 10));  // vflip: row 7 first
    CHECK(px(v, 16, 7) == color(v, 32 + 1));
    CHECK(px(v, 0, 3) == color(v, 16 + 9));
}

TEST_CASE("Plane sizes wrap; 128-cell planes", "[vdp][render]") {
    Vdp v = makeVdp();
    solidTile(v, 1, 4);
    reg(v, 16, 0x00);  // 32x32: column 32 shows column 0 again
    setCell(v, kPlaneB, 0, 0, entry(1), 32);
    frame(v);
    CHECK(px(v, 0, 0) == color(v, 4));
    CHECK(px(v, 256, 0) == color(v, 4));
    CHECK(px(v, 8, 0) == color(v, 0));
    // Vertical wrap: 32 rows of 8 lines; scrolling down by 256 shows row 0 at the top again.
    vsram(v, 1, 256);
    frame(v);
    CHECK(px(v, 0, 0) == color(v, 4));

    reg(v, 16, 0x03);  // 128x32
    vsram(v, 1, 0);
    setCell(v, kPlaneB, 100, 0, entry(1), 128);
    vram(v, kHscroll + 2, {static_cast<std::uint16_t>((1024 - 800) & 0x3FF)});  // plane B x = screen x + 800
    frame(v);
    CHECK(px(v, 0, 0) == color(v, 4));
    CHECK(px(v, 8, 0) == color(v, 0));
}

TEST_CASE("Horizontal scroll: full, per cell, per line", "[vdp][render][scroll]") {
    Vdp v = makeVdp();
    solidTile(v, 1, 6);
    for (int row = 0; row < 28; ++row) {
        setCell(v, kPlaneA, 0, row, entry(1));
    }
    vram(v, kHscroll, {5});  // full screen: entry 0
    frame(v);
    CHECK(px(v, 4, 50) == color(v, 0));
    CHECK(px(v, 5, 50) == color(v, 6));
    CHECK(px(v, 12, 50) == color(v, 6));
    CHECK(px(v, 13, 50) == color(v, 0));
    CHECK(v.lineInfo()[50].hscrollA == 5);

    reg(v, 11, 0x03);  // per line: entry n for line n
    vram(v, static_cast<std::uint16_t>(kHscroll + 20 * 4), {16});
    frame(v);
    CHECK(px(v, 16, 20) == color(v, 6));
    CHECK(px(v, 0, 20) == color(v, 0));
    CHECK(px(v, 0, 21) == color(v, 6));

    reg(v, 11, 0x02);  // per 8-line cell: lines 16-23 use entry 16
    vram(v, static_cast<std::uint16_t>(kHscroll + 16 * 4), {32});
    frame(v);
    CHECK(px(v, 32, 23) == color(v, 6));
    CHECK(px(v, 0, 23) == color(v, 0));
    CHECK(px(v, 0, 24) == color(v, 6));
}

TEST_CASE("Vertical scroll: full screen and per 2-cell column", "[vdp][render][scroll]") {
    Vdp v = makeVdp();
    solidTile(v, 1, 7);
    for (int col = 0; col < 40; ++col) {
        setCell(v, kPlaneA, col, 1, entry(1));  // row 1 = lines 8-15
    }
    vsram(v, 0, 4);
    frame(v);
    CHECK(px(v, 0, 3) == color(v, 0));
    CHECK(px(v, 0, 4) == color(v, 7));
    CHECK(px(v, 0, 11) == color(v, 7));
    CHECK(px(v, 0, 12) == color(v, 0));

    reg(v, 11, 0x04);  // 2-cell columns: column 1 (x 16-31) uses VSRAM entry 2
    vsram(v, 0, 0);
    vsram(v, 2, 8);
    frame(v);
    CHECK(px(v, 15, 8) == color(v, 7));
    CHECK(px(v, 16, 8) == color(v, 0));
    CHECK(px(v, 16, 0) == color(v, 7));
    CHECK(px(v, 32, 8) == color(v, 7));
}

TEST_CASE("Window plane replaces plane A in its region and ignores scrolling", "[vdp][render][window]") {
    Vdp v = makeVdp();
    solidTile(v, 1, 2);
    solidTile(v, 2, 9);
    for (int row = 0; row < 28; ++row) {
        for (int col = 0; col < 40; ++col) {
            setCell(v, kPlaneA, col, row, entry(1));
            setCell(v, kWindow, col, row, entry(2));
        }
    }
    vram(v, kHscroll, {3});
    vsram(v, 0, 5);
    reg(v, 17, 0x02);  // window left of x = 32
    reg(v, 18, 0x00);
    frame(v);
    CHECK(px(v, 0, 0) == color(v, 9));
    CHECK(px(v, 31, 100) == color(v, 9));
    CHECK(px(v, 32, 100) == color(v, 2));

    reg(v, 17, 0x81);  // window right of x = 16
    frame(v);
    CHECK(px(v, 15, 100) == color(v, 2));
    CHECK(px(v, 16, 100) == color(v, 9));

    reg(v, 17, 0x00);
    reg(v, 18, 0x84);  // window below line 32: whole lines
    frame(v);
    CHECK(px(v, 100, 31) == color(v, 2));
    CHECK(px(v, 100, 32) == color(v, 9));
    reg(v, 18, 0x04);  // window above line 32
    frame(v);
    CHECK(px(v, 100, 31) == color(v, 9));
    CHECK(px(v, 100, 32) == color(v, 2));

    v.layers().window = false;  // debug toggle
    frame(v);
    CHECK(px(v, 100, 0) == color(v, 0));
}

TEST_CASE("Sprites: link list, sizes, flips, per-line limits, x = 0 masking, collision",
          "[vdp][render][sprites]") {
    Vdp v = makeVdp();
    solidTile(v, 1, 1);
    solidTile(v, 2, 2);
    solidTile(v, 3, 3);
    solidTile(v, 4, 4);
    // Sprite 0: 2x2 cells at (10, 20), tiles 1-4 column-major, linked to sprite 1.
    sprite(v, 0, 10, 20, 2, 2, 1, entry(1));
    // Sprite 1: 1x1 at (12, 22), drawn behind sprite 0; links to 0 = end.
    sprite(v, 1, 12, 22, 1, 1, 0, entry(4));
    frame(v);
    CHECK(px(v, 10, 20) == color(v, 1));
    CHECK(px(v, 10, 28) == color(v, 2));  // second cell of the first column
    CHECK(px(v, 18, 20) == color(v, 3));
    CHECK(px(v, 18, 28) == color(v, 4));
    CHECK(px(v, 9, 20) == color(v, 0));
    CHECK(px(v, 26, 20) == color(v, 0));
    CHECK(v.lineInfo()[22].sprites == 2);

    // Flipped: hflip swaps the columns, vflip the rows.
    sprite(v, 0, 10, 20, 2, 2, 0, entry(1, 0, false, true, true));
    frame(v);
    CHECK(px(v, 10, 20) == color(v, 4));
    CHECK(px(v, 18, 28) == color(v, 1));

    // Collision flag: two opaque sprite pixels on the same spot.
    sprite(v, 0, 10, 20, 2, 2, 1, entry(1));
    frame(v);
    CHECK((v.readStatus(0) & 0x0020) != 0);

    // Per-line limit: 21 sprites on one line in H40 -> overflow, the 21st is not drawn.
    Vdp o = makeVdp();
    solidTile(o, 1, 5);
    for (int i = 0; i < 21; ++i) {
        sprite(o, i, i * 12, 50, 1, 1, i + 1 < 21 ? i + 1 : 0, entry(1));
    }
    frame(o);
    CHECK(px(o, 19 * 12, 50) == color(o, 5));
    CHECK(px(o, 20 * 12, 50) == color(o, 0));
    CHECK((o.readStatus(0) & 0x0040) != 0);

    // Masking: a sprite at x = 0 (raw) after a visible sprite hides the rest of the line.
    Vdp m = makeVdp();
    solidTile(m, 1, 5);
    sprite(m, 0, 100, 60, 1, 1, 1, entry(1));
    sprite(m, 1, -128, 60, 1, 1, 2, entry(1));
    sprite(m, 2, 200, 60, 1, 1, 0, entry(1));
    frame(m);
    CHECK(px(m, 100, 60) == color(m, 5));
    CHECK(px(m, 200, 60) == color(m, 0));
    // ... but not when it is the first sprite on the line.
    sprite(m, 0, 100, 0, 1, 1, 1, entry(1));
    frame(m);
    CHECK(px(m, 200, 60) == color(m, 5));
}

TEST_CASE("Priority: B low < A low < sprite low < B high < A high < sprite high", "[vdp][render][priority]") {
    Vdp v = makeVdp();
    solidTile(v, 1, 1);
    solidTile(v, 2, 2);
    solidTile(v, 3, 3);
    const auto check = [&](bool bHigh, bool aHigh, bool sHigh, int expected) {
        setCell(v, kPlaneB, 0, 0, entry(1, 0, bHigh));
        setCell(v, kPlaneA, 0, 0, entry(2, 0, aHigh));
        sprite(v, 0, 0, 0, 1, 1, 0, entry(3, 0, sHigh));
        frame(v);
        CHECK(px(v, 0, 0) == color(v, expected));
    };
    check(false, false, false, 3);
    check(true, false, false, 1);
    check(false, true, false, 2);
    check(true, true, false, 2);
    check(true, false, true, 3);
    check(false, true, true, 3);
    check(true, true, true, 3);
    // Layer toggles and priority filters.
    v.layers().sprites = false;
    frame(v);
    CHECK(px(v, 0, 0) == color(v, 2));
    v.layers().planeA = false;
    frame(v);
    CHECK(px(v, 0, 0) == color(v, 1));
    v.layers() = LayerMask{};
    v.layers().priority = LayerMask::Priority::LowOnly;
    frame(v);
    CHECK(px(v, 0, 0) == color(v, 0));
}

TEST_CASE("Shadow / highlight: plane priority shading and sprite operators", "[vdp][render][priority]") {
    Vdp v = makeVdp();
    reg(v, 12, 0x89);  // H40 + shadow/highlight
    solidTile(v, 1, 5);
    solidTile(v, 2, 14);
    solidTile(v, 3, 15);
    solidTile(v, 4, 6);
    for (int col = 0; col < 40; ++col) {
        setCell(v, kPlaneB, col, 0, entry(1, 0, false));
        setCell(v, kPlaneB, col, 1, entry(1, 0, true));
    }
    frame(v);
    CHECK(px(v, 0, 0) == color(v, 5, Intensity::Shadow));  // low-priority planes: shadowed
    CHECK(px(v, 0, 8) == color(v, 5));                     // high priority: normal
    // Palette 3 color 14 = highlight operator, color 15 = shadow operator.
    sprite(v, 0, 0, 0, 1, 1, 1, entry(2, 3));
    sprite(v, 1, 8, 8, 1, 1, 2, entry(2, 3, true));
    sprite(v, 2, 16, 0, 1, 2, 3, entry(4, 1, false));
    sprite(v, 3, 24, 0, 1, 2, 4, entry(4, 1, true));
    sprite(v, 4, 32, 8, 1, 1, 5, entry(3, 3, true));
    sprite(v, 5, 40, 8, 1, 1, 0, entry(3, 3, false));
    frame(v);
    CHECK(px(v, 0, 0) == color(v, 5));                        // shadow + highlight = normal
    CHECK(px(v, 8, 8) == color(v, 5, Intensity::Highlight));  // highlight on a normal pixel
    CHECK(px(v, 32, 8) == color(v, 5, Intensity::Shadow));    // shadow operator
    CHECK(px(v, 40, 8) == color(v, 5));  // a low-priority operator under a high-priority plane does nothing
    CHECK(px(v, 16, 0) == color(v, 22, Intensity::Shadow));  // low sprite over low planes: shadowed
    CHECK(px(v, 24, 0) == color(v, 22));                     // high sprite: normal
    reg(v, 12, 0x81);
    frame(v);
    CHECK(px(v, 0, 0) == color(v, 62));  // without S/H the operators are ordinary colors
}

TEST_CASE("Mid-frame CRAM writes land on the following lines", "[vdp][render]") {
    Vdp v = makeVdp();
    reg(v, 7, 0x01);
    for (int line = 0; line < 262; ++line) {
        v.beginLine(line);
        if (line == 100) {
            cram(v, 1, 0x0E00);
        }
        v.endLine(line);
    }
    CHECK(px(v, 0, 99) == cramToRgba(0x0002));
    CHECK(px(v, 0, 100) == cramToRgba(0x0E00));
    CHECK(v.lineInfo()[100].cramWrites == 1);
    CHECK(v.lineInfo()[99].cramWrites == 0);
}

TEST_CASE("Interlace mode 2: 8x16 tiles, 448-line frames, one field per frame", "[vdp][render][interlace]") {
    Vdp v = makeVdp();
    reg(v, 12, 0x87);  // H40 + interlace mode 2
    // Tile 1 (64 bytes): rows 0-7 color 3, rows 8-15 color 4.
    command(v, 0x01, 64);
    for (int i = 0; i < 32; ++i) {
        v.writeData(i < 16 ? 0x3333 : 0x4444);
    }
    setCell(v, kPlaneA, 0, 0, entry(1));
    frame(v);
    CHECK(v.frameHeight() == 448);
    const int field = v.state().oddFrame ? 1 : 0;
    // Output row 2*line + field shows tile row 2*line + field.
    CHECK(px(v, 0, field) == color(v, 3));
    CHECK(px(v, 0, 6 + field) == color(v, 3));
    CHECK(px(v, 0, 8 + field) == color(v, 4));
    CHECK(px(v, 0, 16 + field) == color(v, 0));
    frame(v);  // the other field fills the other rows
    const int other = v.state().oddFrame ? 1 : 0;
    CHECK(other != field);
    CHECK(px(v, 0, other) == color(v, 3));
    CHECK(px(v, 0, 14 + other) == color(v, 4));
    CHECK((v.readStatus(0) & 0x0010) == (other != 0 ? 0x0010 : 0));

    // Sprites: 16-line cells, Y offset 256.
    command(v, 0x01, kSat);
    v.writeData(256 + 40);
    v.writeData(0x0000);
    v.writeData(entry(1));
    v.writeData(128 + 50);
    frame(v);
    const int f = v.state().oddFrame ? 1 : 0;
    CHECK(px(v, 50, 40 + f) == color(v, 3));
    CHECK(px(v, 50, 48 + f) == color(v, 4));
    CHECK(px(v, 50, 56 + f) == color(v, 0));
}
