#include "common/png.hpp"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <span>

#include "core/util/hash.hpp"

namespace gen::common {

namespace {

void put32(std::vector<std::uint8_t>& out, std::uint32_t v) {
    out.push_back(static_cast<std::uint8_t>(v >> 24));
    out.push_back(static_cast<std::uint8_t>(v >> 16));
    out.push_back(static_cast<std::uint8_t>(v >> 8));
    out.push_back(static_cast<std::uint8_t>(v));
}

void chunk(std::vector<std::uint8_t>& out, const char (&type)[5], const std::vector<std::uint8_t>& data) {
    put32(out, static_cast<std::uint32_t>(data.size()));
    const std::size_t start = out.size();
    out.insert(out.end(), type, type + 4);
    out.insert(out.end(), data.begin(), data.end());
    put32(out, crc32(std::span<const std::uint8_t>(out.data() + start, out.size() - start)));
}

}  // namespace

std::vector<std::uint8_t> encodePng(const std::uint32_t* pixels, int width, int height, int stride) {
    static constexpr std::uint8_t kSignature[] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
    std::vector<std::uint8_t> out(std::begin(kSignature), std::end(kSignature));

    std::vector<std::uint8_t> ihdr;
    put32(ihdr, static_cast<std::uint32_t>(width));
    put32(ihdr, static_cast<std::uint32_t>(height));
    ihdr.insert(ihdr.end(), {8, 6, 0, 0, 0});  // 8-bit RGBA, deflate, adaptive filtering, no interlace
    chunk(out, "IHDR", ihdr);

    // Raw scanlines: filter type 0, then the RGBA bytes.
    std::vector<std::uint8_t> raw;
    raw.reserve((static_cast<std::size_t>(width) * 4 + 1) * static_cast<std::size_t>(height));
    for (int y = 0; y < height; ++y) {
        raw.push_back(0);
        const std::uint32_t* row = pixels + static_cast<std::size_t>(y) * static_cast<std::size_t>(stride);
        for (int x = 0; x < width; ++x) {
            const std::uint32_t p = row[x];
            raw.push_back(static_cast<std::uint8_t>(p));
            raw.push_back(static_cast<std::uint8_t>(p >> 8));
            raw.push_back(static_cast<std::uint8_t>(p >> 16));
            raw.push_back(static_cast<std::uint8_t>(p >> 24));
        }
    }

    // zlib stream of stored deflate blocks, then Adler-32.
    std::vector<std::uint8_t> z = {0x78, 0x01};
    std::size_t pos = 0;
    do {
        const std::size_t n = std::min<std::size_t>(raw.size() - pos, 65535);
        const auto len = static_cast<std::uint16_t>(n);
        z.push_back(pos + n == raw.size() ? 1 : 0);
        z.push_back(static_cast<std::uint8_t>(len));
        z.push_back(static_cast<std::uint8_t>(len >> 8));
        z.push_back(static_cast<std::uint8_t>(~len));
        z.push_back(static_cast<std::uint8_t>(static_cast<std::uint16_t>(~len) >> 8));
        z.insert(z.end(), raw.begin() + static_cast<std::ptrdiff_t>(pos),
                 raw.begin() + static_cast<std::ptrdiff_t>(pos + n));
        pos += n;
    } while (pos < raw.size());
    std::uint32_t a = 1;
    std::uint32_t b = 0;
    for (const std::uint8_t v : raw) {
        a = (a + v) % 65521;
        b = (b + a) % 65521;
    }
    put32(z, (b << 16) | a);
    chunk(out, "IDAT", z);
    chunk(out, "IEND", {});
    return out;
}

bool writePng(const std::filesystem::path& path, const std::uint32_t* pixels, int width, int height,
              int stride) {
    const auto data = encodePng(pixels, width, height, stride);
    std::ofstream f(path, std::ios::binary);
    f.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    return f.good();
}

}  // namespace gen::common
