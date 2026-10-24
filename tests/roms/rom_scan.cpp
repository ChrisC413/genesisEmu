#include "rom_scan.hpp"

#include "core/cart/rom.hpp"

namespace gen::test {

namespace fs = std::filesystem;

std::map<std::string, GameImage> scanRoms(const fs::path& dir) {
    std::map<std::string, GameImage> out;
    std::vector<cart::Rom> sk;
    std::vector<cart::Rom> s3;
    for (const auto& entry : fs::recursive_directory_iterator(dir)) {
        if (!entry.is_regular_file() || entry.file_size() > cart::kMaxRomSize + cart::kSmdHeaderSize) {
            continue;
        }
        auto result = cart::loadRomFile(entry.path());
        if (!result || result.rom->known == nullptr) {
            continue;
        }
        cart::Rom& rom = *result.rom;
        if (rom.known->game == cart::Game::SonicAndKnuckles) {
            sk.push_back(rom);
        } else if (rom.known->game == cart::Game::Sonic3) {
            s3.push_back(rom);
        }
        out.emplace(rom.sha1Hex, GameImage{rom.data, rom.header.regions});
    }
    for (const auto& a : sk) {
        for (const auto& b : s3) {
            if (const auto* combined = cart::identifyLockOn(a.data, b.data)) {
                out.emplace(std::string(combined->sha1),
                            GameImage{cart::lockOnImage(a.data, b.data), b.header.regions});
            }
        }
    }
    return out;
}

}  // namespace gen::test
