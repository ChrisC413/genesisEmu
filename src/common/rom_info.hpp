#pragma once

#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "core/cart/rom.hpp"

namespace gen::common {

struct RomInfoField {
    std::string label;
    std::string value;
};

// The fields shown for a loaded ROM, in display order. Shared by `gen_cli info` and the app's ROM info
// panel so both always show the same thing.
std::vector<RomInfoField> describeRom(const cart::Rom& rom);

nlohmann::json romToJson(const cart::Rom& rom);

}  // namespace gen::common
