#include "core/util/hash.hpp"

#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

namespace {

std::span<const std::uint8_t> bytes(std::string_view s) {
    return {reinterpret_cast<const std::uint8_t*>(s.data()), s.size()};
}

std::string sha1Hex(std::span<const std::uint8_t> data) {
    return gen::toHex(gen::sha1(data));
}

}  // namespace

TEST_CASE("SHA-1 matches FIPS 180 test vectors", "[hash]") {
    CHECK(sha1Hex(bytes("")) == "da39a3ee5e6b4b0d3255bfef95601890afd80709");
    CHECK(sha1Hex(bytes("abc")) == "a9993e364706816aba3e25717850c26c9cd0d89d");
    CHECK(sha1Hex(bytes("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq")) ==
          "84983e441c3bd26ebaae4aa1f95129e5e54670f1");
    const std::vector<std::uint8_t> million(1'000'000, 'a');
    CHECK(sha1Hex(million) == "34aa973cd4c4daa4f61eeb2bdbad27316534016f");
}

TEST_CASE("CRC32 matches the standard check value", "[hash]") {
    CHECK(gen::crc32(bytes("123456789")) == 0xCBF43926u);
    CHECK(gen::crc32(bytes("")) == 0u);
}
