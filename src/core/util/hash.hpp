#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

namespace gen {

using Sha1Digest = std::array<std::uint8_t, 20>;

Sha1Digest sha1(std::span<const std::uint8_t> data);
std::uint32_t crc32(std::span<const std::uint8_t> data);

// Lowercase hex, e.g. "69e102855d4389c3fd1a8f3dc7d193f8eee5fe5b".
std::string toHex(std::span<const std::uint8_t> bytes);

}  // namespace gen
