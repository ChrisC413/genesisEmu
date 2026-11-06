#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "core/cart/rom.hpp"

namespace gen::vdp {
class Vdp;
}

namespace gen::app {

// Window names double as ImGui IDs and as keys in the persisted dock layout; do not rename casually.
inline constexpr const char* kGameViewWindow = "Game View";
inline constexpr const char* kRomInfoWindow = "ROM Info";

struct GameViewSettings {
    enum class Scale : std::uint8_t { Fit, Integer };
    Scale scale = Scale::Fit;
    bool aspectCorrect = true;  // show the picture at 4:3 instead of square pixels
    bool linearFilter = false;
    bool showOverlay = true;  // frame-time overlay
};

// The emulated screen (M03-10): the VDP framebuffer as an OpenGL texture, scaled into the panel.
class GameView {
public:
    GameView() = default;
    ~GameView();
    GameView(const GameView&) = delete;
    GameView& operator=(const GameView&) = delete;

    // Copies the visible part of the framebuffer into the texture (needs the GL context).
    void upload(const vdp::Vdp& vdp);
    void clear() { hasFrame_ = false; }
    // `status` is shown when there is no frame; `overlay` is the frame-time text.
    // `boxes` (x, y, w, h, IM_COL32 colour in emulated screen pixels) are outlined over the picture.
    struct Box {
        int x, y, w, h;
        std::uint32_t colour;
    };
    void draw(bool* open, const std::optional<cart::Rom>& rom, const std::string& overlay,
              const std::vector<Box>& boxes = {});

    GameViewSettings& settings() { return settings_; }
    bool hasFrame() const { return hasFrame_; }
    std::uint64_t uploads() const { return uploads_; }

private:
    unsigned texture_ = 0;
    int width_ = 320;
    int height_ = 224;
    bool hasFrame_ = false;
    bool linearApplied_ = false;
    std::uint64_t uploads_ = 0;
    GameViewSettings settings_;
};

// Parsed header, hashes and identification of the loaded ROM (same fields as `gen_cli info`).
void drawRomInfo(bool* open, const std::optional<cart::Rom>& rom);

}  // namespace gen::app
