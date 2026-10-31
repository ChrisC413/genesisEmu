// Minimal PNG writer used for frame dumps and golden-frame diff images (M03-11).

#include "common/png.hpp"

#include <array>
#include <cstdint>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("PNG writer produces a well-formed RGBA image with stored deflate blocks", "[png]") {
    const std::array<std::uint32_t, 6> pixels = {0xFF0000FF, 0xFF00FF00, 0xFFFF0000,
                                                 0x80FFFFFF, 0xFF000000, 0xFF123456};
    const auto png = gen::common::encodePng(pixels.data(), 2, 2, 3);  // stride 3: the last column is skipped
    REQUIRE(png.size() > 8 + 25 + 12 + 12);
    CHECK(png[0] == 0x89);
    CHECK(png[1] == 'P');
    // IHDR: 2x2, 8-bit RGBA.
    CHECK(png[12] == 'I');
    CHECK(png[19] == 2);
    CHECK(png[23] == 2);
    CHECK(png[24] == 8);
    CHECK(png[25] == 6);
    // IDAT holds a zlib header, one final stored block of 2 * (1 + 8) bytes, the raw rows and Adler-32.
    const std::size_t idat = 8 + 25;
    CHECK(png[idat + 4] == 'I');
    CHECK(png[idat + 8] == 0x78);
    CHECK(png[idat + 10] == 1);
    CHECK(png[idat + 11] == 18);
    CHECK(png[idat + 15] == 0);     // filter byte of row 0
    CHECK(png[idat + 16] == 0xFF);  // R of pixel 0
    CHECK(png[idat + 19] == 0xFF);  // A of pixel 0
    CHECK(png[idat + 24] == 0);     // filter byte of row 1
    CHECK(png[idat + 25] == 0xFF);  // R of pixel (0,1) = 0x80FFFFFF
    CHECK(png[idat + 28] == 0x80);
    // IEND with its fixed CRC.
    const std::size_t end = png.size() - 12;
    CHECK(png[end + 4] == 'I');
    CHECK(png[end + 8] == 0xAE);
    CHECK(png[end + 11] == 0x82);
}
