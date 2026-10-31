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
    const std::array<PadType, 2> types = s_.padType;
    s_ = IoState{};
    s_.buttons = buttons;
    s_.padType = types;
    // Bit 7 overseas, bit 6 PAL, bit 5 = 1 (no expansion unit), bits 0-3 hardware version (0 = no TMSS).
    s_.version = static_cast<std::uint8_t>((region != Region::Japan ? 0x80 : 0) | (pal ? 0x40 : 0) | 0x20 |
                                           (tmss ? 0x01 : 0x00));
    for (int i = 0; i < 3; ++i) {
        s_.serial[static_cast<std::size_t>(i * 3)] = 0xFF;  // TxData
    }
}

void Io::updateTh(int port, std::uint64_t mclk) {
    const auto p = static_cast<std::size_t>(port);
    // TH is driven by the console when it is an output, otherwise pulled high.
    const bool th = (s_.control[p] & 0x40) == 0 || (s_.data[p] & 0x40) != 0;
    if (mclk - s_.thChangedAt[p] > kSixButtonTimeoutMclk) {
        s_.thCount[p] = 0;
    }
    if (th != s_.th[p]) {
        if (!th && s_.thCount[p] < 4) {
            ++s_.thCount[p];
        }
        s_.th[p] = th;
        s_.thChangedAt[p] = mclk;
    }
}

std::uint8_t Io::readData(int port, std::uint64_t mclk) {
    const auto p = static_cast<std::size_t>(port);
    const std::uint8_t ctrl = s_.control[p];
    const std::uint8_t latch = s_.data[p];
    std::uint8_t pins = 0x7F;  // nothing connected (port 3): inputs pulled high
    if (port < 2 && s_.padType[p] != PadType::None) {
        updateTh(port, mclk);
        const std::uint16_t b = s_.buttons[p];
        const bool th = s_.th[p];
        const int count = s_.padType[p] == PadType::SixButton ? s_.thCount[p] : 0;
        const auto bit = [&](std::uint16_t mask, int pos) {
            return static_cast<std::uint8_t>((b & mask) != 0 ? 0 : (1 << pos));
        };
        if (th && count == 3) {  // 6-button, 4th TH-high phase: 1 C B M X Y Z
            pins = static_cast<std::uint8_t>(0x40 | bit(kPadC, 5) | bit(kPadB, 4) | bit(kPadMode, 3) |
                                             bit(kPadX, 2) | bit(kPadY, 1) | bit(kPadZ, 0));
        } else if (th) {  // TH = 1: 1 C B R L D U
            pins = static_cast<std::uint8_t>(0x40 | bit(kPadC, 5) | bit(kPadB, 4) | bit(kPadRight, 3) |
                                             bit(kPadLeft, 2) | bit(kPadDown, 1) | bit(kPadUp, 0));
        } else if (count == 3) {  // 6-button, 3rd TH-low phase: 0 S A 0 0 0 0 (identifies the pad)
            pins = static_cast<std::uint8_t>(bit(kPadStart, 5) | bit(kPadA, 4));
        } else if (count == 4) {  // 6-button, 4th TH-low phase: 0 S A 1 1 1 1
            pins = static_cast<std::uint8_t>(bit(kPadStart, 5) | bit(kPadA, 4) | 0x0F);
        } else {  // TH = 0: 0 S A 0 0 D U
            pins = static_cast<std::uint8_t>(bit(kPadStart, 5) | bit(kPadA, 4) | bit(kPadDown, 1) |
                                             bit(kPadUp, 0));
        }
    }
    // Output pins read back the latch; bit 7 always reads the latch.
    return static_cast<std::uint8_t>((latch & (0x80 | ctrl)) | (pins & ~ctrl & 0x7F));
}

std::uint8_t Io::read(std::uint32_t reg, std::uint64_t mclk) {
    reg &= 0x0F;
    if (reg == 0) {
        return s_.version;
    }
    if (reg <= 3) {
        return readData(static_cast<int>(reg - 1), mclk);
    }
    if (reg <= 6) {
        return s_.control[reg - 4];
    }
    return s_.serial[reg - 7];
}

void Io::write(std::uint32_t reg, std::uint8_t value, std::uint64_t mclk) {
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
        return;
    }
    const auto port = static_cast<int>((reg - 1) % 3);
    if (port < 2) {
        updateTh(port, mclk);
    }
}

}  // namespace gen::bus
