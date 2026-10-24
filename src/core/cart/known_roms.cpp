#include "core/cart/known_roms.hpp"

#include <array>

namespace gen::cart {

namespace {

// Source: No-Intro "Sega - Mega Drive - Genesis" DAT (libretro-database mirror).
constexpr std::array kKnownRoms{
    KnownRom{"6ddb7de1e17e7f6cdb88927bd906352030daa194", 0xF9394E97, 0x080000, Game::Sonic1,
             "Sonic the Hedgehog", "REV00", "USA, Europe"},
    KnownRom{"69e102855d4389c3fd1a8f3dc7d193f8eee5fe5b", 0xAFE05EEE, 0x080000, Game::Sonic1,
             "Sonic the Hedgehog", "REV01", "Japan, Europe, Korea"},
    KnownRom{"14dd06fc3aa19a59a818ea1f6de150c9061b14d4", 0x24AB4C3A, 0x100000, Game::Sonic2,
             "Sonic the Hedgehog 2", "REV00", "World"},
    KnownRom{"8bca5dcef1af3e00098666fd892dc1c2a76333f9", 0x7B905383, 0x100000, Game::Sonic2,
             "Sonic the Hedgehog 2", "REV01", "World"},
    KnownRom{"2af1003247aec262089c8df22d05e80d04a1b5e4", 0xF23AD4B3, 0x100000, Game::Sonic2,
             "Sonic the Hedgehog 2", "REV02", "World (Sonic Compilation)"},
    KnownRom{"75e9c4705259d84112b3e697a6c00a0813d47d71", 0x9BC192CE, 0x200000, Game::Sonic3,
             "Sonic the Hedgehog 3", "REV00", "USA"},
    KnownRom{"2ff45bb056ede0f745e52f8d02c54b4ca724ca4c", 0x6A632503, 0x200000, Game::Sonic3,
             "Sonic the Hedgehog 3", "REV00", "Europe"},
    KnownRom{"7b98b21b7274233e962132bc22a7ccdf548c0ddb", 0xF4951D1F, 0x200000, Game::Sonic3,
             "Sonic the Hedgehog 3", "REV00", "Japan, Korea"},
    KnownRom{"cfbf98c36c776677290a872547ac47c53d2761d6", 0x63522553, 0x400000, Game::Sonic3AndKnuckles,
             "Sonic & Knuckles + Sonic the Hedgehog 3", "REV00", "USA"},
    KnownRom{"45ca797353756787c8abaacbe84ce662166ac0ee", 0x92F0929E, 0x400000, Game::Sonic3AndKnuckles,
             "Sonic & Knuckles + Sonic the Hedgehog 3", "REV00", "Europe"},
    KnownRom{"b711a909cce238ca4af3e517a2edca306228efa5", 0x0C06AA82, 0x400000, Game::Sonic3AndKnuckles,
             "Sonic & Knuckles + Sonic the Hedgehog 3", "REV00", "Japan, Korea"},
};

}  // namespace

std::span<const KnownRom> knownRoms() {
    return kKnownRoms;
}

const KnownRom* identifyRom(std::string_view sha1Hex) {
    for (const KnownRom& rom : kKnownRoms) {
        if (rom.sha1 == sha1Hex) {
            return &rom;
        }
    }
    return nullptr;
}

std::string_view toString(Game game) {
    switch (game) {
        case Game::Sonic1:
            return "Sonic 1";
        case Game::Sonic2:
            return "Sonic 2";
        case Game::Sonic3:
            return "Sonic 3";
        case Game::Sonic3AndKnuckles:
            return "Sonic 3 & Knuckles";
    }
    return "?";
}

}  // namespace gen::cart
