#include "core/util/hash.hpp"

#include <bit>

namespace gen {

namespace {

class Sha1 {
public:
    void update(std::span<const std::uint8_t> data) {
        for (std::uint8_t byte : data) {
            block_[blockLen_++] = byte;
            if (blockLen_ == block_.size()) {
                compress();
                blockLen_ = 0;
            }
        }
        totalBits_ += static_cast<std::uint64_t>(data.size()) * 8;
    }

    Sha1Digest finish() {
        const std::uint64_t bits = totalBits_;
        const std::uint8_t pad = 0x80;
        update({&pad, 1});
        const std::uint8_t zero = 0;
        while (blockLen_ != 56) {
            update({&zero, 1});
        }
        std::array<std::uint8_t, 8> length{};
        for (int i = 0; i < 8; ++i) {
            length[i] = static_cast<std::uint8_t>(bits >> (56 - 8 * i));
        }
        update(length);

        Sha1Digest digest{};
        for (std::size_t i = 0; i < 5; ++i) {
            for (std::size_t j = 0; j < 4; ++j) {
                digest[i * 4 + j] = static_cast<std::uint8_t>(h_[i] >> (24 - 8 * j));
            }
        }
        return digest;
    }

private:
    void compress() {
        std::array<std::uint32_t, 80> w{};
        for (std::size_t i = 0; i < 16; ++i) {
            w[i] = (std::uint32_t{block_[i * 4]} << 24) | (std::uint32_t{block_[i * 4 + 1]} << 16) |
                   (std::uint32_t{block_[i * 4 + 2]} << 8) | std::uint32_t{block_[i * 4 + 3]};
        }
        for (std::size_t i = 16; i < 80; ++i) {
            w[i] = std::rotl(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
        }

        std::uint32_t a = h_[0], b = h_[1], c = h_[2], d = h_[3], e = h_[4];
        for (std::size_t i = 0; i < 80; ++i) {
            std::uint32_t f = 0;
            std::uint32_t k = 0;
            if (i < 20) {
                f = (b & c) | (~b & d);
                k = 0x5A827999;
            } else if (i < 40) {
                f = b ^ c ^ d;
                k = 0x6ED9EBA1;
            } else if (i < 60) {
                f = (b & c) | (b & d) | (c & d);
                k = 0x8F1BBCDC;
            } else {
                f = b ^ c ^ d;
                k = 0xCA62C1D6;
            }
            const std::uint32_t temp = std::rotl(a, 5) + f + e + k + w[i];
            e = d;
            d = c;
            c = std::rotl(b, 30);
            b = a;
            a = temp;
        }
        h_[0] += a;
        h_[1] += b;
        h_[2] += c;
        h_[3] += d;
        h_[4] += e;
    }

    std::array<std::uint32_t, 5> h_{0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0};
    std::array<std::uint8_t, 64> block_{};
    std::size_t blockLen_ = 0;
    std::uint64_t totalBits_ = 0;
};

constexpr std::array<std::uint32_t, 256> makeCrcTable() {
    std::array<std::uint32_t, 256> table{};
    for (std::uint32_t i = 0; i < 256; ++i) {
        std::uint32_t c = i;
        for (int k = 0; k < 8; ++k) {
            c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
        }
        table[i] = c;
    }
    return table;
}

constexpr auto kCrcTable = makeCrcTable();

}  // namespace

Sha1Digest sha1(std::span<const std::uint8_t> data) {
    Sha1 s;
    s.update(data);
    return s.finish();
}

std::uint32_t crc32(std::span<const std::uint8_t> data) {
    std::uint32_t c = 0xFFFFFFFFu;
    for (std::uint8_t byte : data) {
        c = kCrcTable[(c ^ byte) & 0xFF] ^ (c >> 8);
    }
    return c ^ 0xFFFFFFFFu;
}

std::string toHex(std::span<const std::uint8_t> bytes) {
    static constexpr char kDigits[] = "0123456789abcdef";
    std::string out;
    out.reserve(bytes.size() * 2);
    for (std::uint8_t b : bytes) {
        out.push_back(kDigits[b >> 4]);
        out.push_back(kDigits[b & 0xF]);
    }
    return out;
}

}  // namespace gen
