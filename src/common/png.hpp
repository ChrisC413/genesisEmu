#pragma once
// Minimal PNG writer for headless frame dumps and test diff images: 8-bit RGBA, no filtering, zlib
// "stored" deflate blocks (no compression dependency; files are larger but trivially correct).

#include <cstdint>
#include <filesystem>
#include <vector>

namespace gen::common {

// `pixels` is RGBA8888 in byte order R, G, B, A; `stride` is the row pitch in pixels.
std::vector<std::uint8_t> encodePng(const std::uint32_t* pixels, int width, int height, int stride);
bool writePng(const std::filesystem::path& path, const std::uint32_t* pixels, int width, int height, int stride);

}  // namespace gen::common
