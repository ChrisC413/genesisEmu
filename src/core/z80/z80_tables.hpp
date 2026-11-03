#pragma once
// Flag lookup tables shared by the Z80 core: S, Z, Y and X of a result byte, with and without parity.

#include <array>
#include <cstdint>

#include "core/z80/z80.hpp"

namespace gen::z80 {

constexpr std::array<std::uint8_t, 256> makeSz53(bool parity) {
    std::array<std::uint8_t, 256> t{};
    for (int v = 0; v < 256; ++v) {
        auto f = static_cast<std::uint8_t>(v & (kFlagS | kFlagY | kFlagX));
        if (v == 0) {
            f |= kFlagZ;
        }
        if (parity) {
            int bits = 0;
            for (int b = 0; b < 8; ++b) {
                bits += (v >> b) & 1;
            }
            if ((bits & 1) == 0) {
                f |= kFlagP;
            }
        }
        t[static_cast<std::size_t>(v)] = f;
    }
    return t;
}

inline constexpr std::array<std::uint8_t, 256> kSz53 = makeSz53(false);
inline constexpr std::array<std::uint8_t, 256> kSzp = makeSz53(true);

}  // namespace gen::z80
