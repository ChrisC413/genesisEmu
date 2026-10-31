#pragma once

#include <filesystem>
#include <optional>
#include <string>

#include "app/platform/file_dialog.hpp"
#include "app/platform/window.hpp"
#include "common/config.hpp"
#include "common/paths.hpp"
#include "core/cart/rom.hpp"

namespace gen::app {

enum AppExit : int {
    kAppOk = 0,
    kAppUsage = 1,
    kAppRomLoadFailed = 2,
    kAppSmokeCheckFailed = 3,
    kAppNoGraphics = 77,  // CTest SKIP_RETURN_CODE: no OpenGL 3.3 (e.g. headless CI runner)
};

struct AppOptions {
    std::optional<std::filesystem::path> rom;
    int smokeFrames = 0;                // > 0: render this many frames, run self-checks, exit
    bool expectRestoredLayout = false;  // smoke check: a saved dock layout must have been loaded
    std::optional<std::filesystem::path> screenshot;  // smoke test: save the last frame as PNG
};

class App {
public:
    explicit App(common::AppPaths paths);
    int run(const AppOptions& options);

private:
    bool openRom(const std::filesystem::path& path);
    void addRecentRom(const std::filesystem::path& path);
    void drawDockSpace();
    void drawMenuBar();
    void drawPopups();
    void persistLayout();
    void saveConfig();
    int smokeCheck(const AppOptions& options) const;

    common::AppPaths paths_;
    common::ConfigFile config_;
    Window window_;
    RomFileDialog romDialog_;
    std::optional<cart::Rom> rom_;
    std::filesystem::path romPath_;

    bool quit_ = false;
    bool layoutLoaded_ = false;
    bool layoutRestored_ = false;  // a saved layout was found at startup
    bool resetLayout_ = false;
    bool showGameView_ = true;
    bool showRomInfo_ = true;
    bool openAbout_ = false;
    bool screenshotFailed_ = false;
    std::string pendingError_;
};

}  // namespace gen::app
