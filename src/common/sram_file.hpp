#pragma once
// Battery-save persistence (spec 04, M04-08): the SRAM contents as <user data>/roms/<sha1>/<sha1>.srm.

#include <filesystem>
#include <string_view>

#include "common/paths.hpp"
#include "core/cart/sram.hpp"

namespace gen::common {

inline std::filesystem::path sramPath(const AppPaths& paths, std::string_view sha1Hex) {
    return paths.romDataDir(sha1Hex) / (std::string(sha1Hex) + ".srm");
}

// Loads `path` into the SRAM when it exists. Returns true if a file was loaded.
bool loadSram(cart::SramDevice& sram, const std::filesystem::path& path);
// Writes the SRAM (creating directories) and clears its dirty flag. Returns false on I/O errors.
bool saveSram(cart::SramDevice& sram, const std::filesystem::path& path);

}  // namespace gen::common
