#include "core/bus/io.hpp"

#include "core/cart/rom.hpp"

namespace gen::bus {

Region regionFromHeader(std::uint8_t headerRegions) {
    if ((headerRegions & cart::kRegionAmericas) != 0 || headerRegions == cart::kRegionNone) {
        return Region::Americas;
    }
    if ((headerRegions & cart::kRegionJapan) != 0) {
        return Region::Japan;
    }
    return Region::Europe;
}

const char* toString(Region region) {
    switch (region) {
    case Region::Japan:
        return "japan";
    case Region::Americas:
        return "americas";
    case Region::Europe:
        return "europe";
    }
    return "?";
}

void Io::reset(Region region, bool pal, bool tmss) {
    const std::array<std::uint16_t, 2> buttons = s_.buttons;
    s_ = IoState{};
    s_.buttons = buttons;
    // Bit 7 overseas, bit 6 PAL, bit 5 = 1 (no expansion unit), bits 0-3 hardware version (0 = no TMSS).
    s_.version = static_cast<std::uint8_t>((region != Region::Japan ? 0x80 : 0) | (pal ? 0x40 : 0) | 0x20 |
                                           (tmss ? 0x01 : 0x00));
    for (int i = 0; i < 3; ++i) {
        s_.serial[static_cast<std::size_t>(i * 3)] = 0xFF;  // TxData
    }
}

std::uint8_t Io::readData(int port) const {
    const auto p = static_cast<std::size_t>(port);
    const std::uint8_t ctrl = s_.control[p];
    const std::uint8_t latch = s_.data[p];
    std::uint8_t pins = 0x7F;  // nothing connected (port 3): inputs pulled high
    if (port < 2) {
        const std::uint16_t b = s_.buttons[p];
        const bool th = (ctrl & 0x40) != 0 ? (latch & 0x40) != 0 : true;
        const auto bit = [&](std::uint16_t mask, int pos) {
            return static_cast<std::uint8_t>((b & mask) != 0 ? 0 : (1 << pos));
        };
        if (th) {  // TH = 1: 1 C B R L D U
            pins = static_cast<std::uint8_t>(0x40 | bit(kPadC, 5) | bit(kPadB, 4) | bit(kPadRight, 3) |
                                             bit(kPadLeft, 2) | bit(kPadDown, 1) | bit(kPadUp, 0));
        } else {  // TH = 0: 0 Start A 0 0 D U
            pins = static_cast<std::uint8_t>(bit(kPadStart, 5) | bit(kPadA, 4) | bit(kPadDown, 1) |
                                             bit(kPadUp, 0));
        }
    }
    // Output pins read back the latch; bit 7 always reads the latch.
    return static_cast<std::uint8_t>((latch & (0x80 | ctrl)) | (pins & ~ctrl & 0x7F));
}

std::uint8_t Io::read(std::uint32_t reg) const {
    reg &= 0x0F;
    if (reg == 0) {
        return s_.version;
    }
    if (reg <= 3) {
        return readData(static_cast<int>(reg - 1));
    }
    if (reg <= 6) {
        return s_.control[reg - 4];
    }
    return s_.serial[reg - 7];
}

void Io::write(std::uint32_t reg, std::uint8_t value) {
    reg &= 0x0F;
    if (reg == 0) {
        return;  // version is read-only
    }
    if (reg <= 3) {
        s_.data[reg - 1] = value;
    } else if (reg <= 6) {
        s_.control[reg - 4] = value;
    } else {
        s_.serial[reg - 7] = value;
    }
}

}  // namespace gen::bus
