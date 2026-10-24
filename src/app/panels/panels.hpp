#pragma once

#include <optional>

#include "core/cart/rom.hpp"

namespace gen::app {

// Window names double as ImGui IDs and as keys in the persisted dock layout; do not rename casually.
inline constexpr const char* kGameViewWindow = "Game View";
inline constexpr const char* kRomInfoWindow = "ROM Info";

// The emulated screen. Until the VDP exists (M03) it shows a black 4:3 frame and the ROM status.
void drawGameView(bool* open, const std::optional<cart::Rom>& rom);

// Parsed header, hashes and identification of the loaded ROM (same fields as `gen_cli info`).
void drawRomInfo(bool* open, const std::optional<cart::Rom>& rom);

}  // namespace gen::app
