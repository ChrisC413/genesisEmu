#include "common/sram_file.hpp"

#include <fstream>
#include <iterator>
#include <system_error>
#include <vector>

namespace gen::common {

bool loadSram(cart::SramDevice& sram, const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return false;
    }
    const std::vector<char> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    sram.load(std::span(reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size()));
    return true;
}

bool saveSram(cart::SramDevice& sram, const std::filesystem::path& path) {
    std::error_code ec;
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path(), ec);
    }
    // Write a temporary file and rename it so a crash never leaves a truncated save.
    const auto tmp = std::filesystem::path(path).concat(".tmp");
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        const auto& d = sram.data();
        out.write(reinterpret_cast<const char*>(d.data()), static_cast<std::streamsize>(d.size()));
        if (!out) {
            return false;
        }
    }
    std::filesystem::rename(tmp, path, ec);
    if (ec) {
        return false;
    }
    sram.clearDirty();
    return true;
}

}  // namespace gen::common
