#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace gen::test {

// Builds legally distributable stand-in ROM images with a valid Genesis header. No commercial data.
struct SyntheticRomOptions {
    std::size_t size = 0x20000;
    std::string consoleName = "SEGA MEGA DRIVE";
    std::string copyright = "(C)TEST 2026.OCT";
    std::string domesticTitle = "SYNTHETIC TEST ROM";
    std::string overseasTitle = "SYNTHETIC TEST ROM";
    std::string serial = "GM 00000000-00";
    std::string ioSupport = "J";
    std::string regionCodes = "JUE";
    bool declareSram = false;
    std::uint8_t sramType = 0xF8;
    std::uint32_t sramStart = 0x200001;
    std::uint32_t sramEnd = 0x203FFF;
    bool correctChecksum = true;
    std::uint32_t seed = 1;  // fills the body with deterministic pseudo-random bytes
};

std::vector<std::uint8_t> makeSyntheticRom(const SyntheticRomOptions& options = {});

// Encodes a plain image as an SMD file (512-byte header + interleaved 16 KB blocks). The image size must be
// a multiple of 16 KB.
std::vector<std::uint8_t> encodeSmd(std::span<const std::uint8_t> image);

}  // namespace gen::test
