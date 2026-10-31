#include "synthetic_rom.hpp"

#include <algorithm>
#include <stdexcept>

#include "core/cart/rom.hpp"

namespace gen::test {

namespace {

void putText(std::vector<std::uint8_t>& rom, std::size_t at, std::size_t len, const std::string& text) {
    for (std::size_t i = 0; i < len; ++i) {
        rom[at + i] = i < text.size() ? static_cast<std::uint8_t>(text[i]) : ' ';
    }
}

void putBe32(std::vector<std::uint8_t>& rom, std::size_t at, std::uint32_t v) {
    for (std::size_t i = 0; i < 4; ++i) {
        rom[at + i] = static_cast<std::uint8_t>(v >> (24 - 8 * i));
    }
}

}  // namespace

std::vector<std::uint8_t> makeSyntheticRom(const SyntheticRomOptions& o) {
    if (o.size < 0x200) {
        // Too small to hold a header: return deterministic filler for size-limit tests.
        return std::vector<std::uint8_t>(o.size, 0x4E);
    }
    std::vector<std::uint8_t> rom(o.size, 0);

    std::uint32_t state = o.seed * 2654435761u + 1;
    for (std::size_t i = 0x200; i < rom.size(); ++i) {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        rom[i] = static_cast<std::uint8_t>(state);
    }

    putBe32(rom, 0x000, 0x00FFFE00);  // initial SSP
    putBe32(rom, 0x004, 0x00000200);  // reset PC
    putText(rom, 0x100, 16, o.consoleName);
    putText(rom, 0x110, 16, o.copyright);
    putText(rom, 0x120, 48, o.domesticTitle);
    putText(rom, 0x150, 48, o.overseasTitle);
    putText(rom, 0x180, 14, o.serial);
    putText(rom, 0x190, 16, o.ioSupport);
    putBe32(rom, 0x1A0, 0);
    putBe32(rom, 0x1A4, static_cast<std::uint32_t>(o.size - 1));
    putBe32(rom, 0x1A8, 0xFF0000);
    putBe32(rom, 0x1AC, 0xFFFFFF);
    if (o.declareSram) {
        rom[0x1B0] = 'R';
        rom[0x1B1] = 'A';
        rom[0x1B2] = o.sramType;
        rom[0x1B3] = 0x20;
        putBe32(rom, 0x1B4, o.sramStart);
        putBe32(rom, 0x1B8, o.sramEnd);
    } else {
        putText(rom, 0x1B0, 12, "");
    }
    putText(rom, 0x1BC, 12, "");
    putText(rom, 0x1C8, 40, "");
    putText(rom, 0x1F0, 16, o.regionCodes);

    std::uint16_t sum = cart::computeChecksum(rom);
    if (!o.correctChecksum) {
        sum = static_cast<std::uint16_t>(sum ^ 0x5A5A);
    }
    rom[0x18E] = static_cast<std::uint8_t>(sum >> 8);
    rom[0x18F] = static_cast<std::uint8_t>(sum);
    return rom;
}

std::vector<std::uint8_t> encodeSmd(std::span<const std::uint8_t> image) {
    if (image.size() % cart::kSmdBlockSize != 0) {
        throw std::invalid_argument("SMD images must be a multiple of 16 KB");
    }
    std::vector<std::uint8_t> file(cart::kSmdHeaderSize + image.size(), 0);
    file[0] = static_cast<std::uint8_t>(std::min<std::size_t>(image.size() / cart::kSmdBlockSize, 0xFF));
    file[1] = 0x03;  // "binary" type
    file[8] = 0xAA;
    file[9] = 0xBB;
    file[10] = 0x06;

    constexpr std::size_t half = cart::kSmdBlockSize / 2;
    for (std::size_t block = 0; block < image.size(); block += cart::kSmdBlockSize) {
        std::uint8_t* out = file.data() + cart::kSmdHeaderSize + block;
        for (std::size_t i = 0; i < half; ++i) {
            out[i] = image[block + i * 2 + 1];
            out[half + i] = image[block + i * 2];
        }
    }
    return file;
}

}  // namespace gen::test
