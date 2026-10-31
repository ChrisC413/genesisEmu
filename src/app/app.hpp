#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>

#include "app/panels/panels.hpp"
#include "app/platform/file_dialog.hpp"
#include "app/platform/window.hpp"
#include "common/config.hpp"
#include "common/paths.hpp"
#include "core/cart/rom.hpp"
#include "core/sched/machine.hpp"

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
    std::optional<std::filesystem::path> lockOn;      // cartridge locked onto the ROM (S&K + Sonic 3)
    bool fastForward = false;                         // start in fast-forward (benchmarks, smoke tests)
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
    void startMachine();
    void stepEmulation(bool smoke);
    std::string overlayText() const;

    common::AppPaths paths_;
    common::ConfigFile config_;
    Window window_;
    RomFileDialog romDialog_;
    std::optional<cart::Rom> rom_;
    std::filesystem::path romPath_;
    std::optional<std::filesystem::path> lockOnPath_;
    std::unique_ptr<Machine> machine_;
    GameView gameView_;

    // Emulation pacing (M03-10): one emulated frame per host vsync when the display runs at ~60 Hz,
    // otherwise an accumulator against the 59.92 / 49.70 Hz frame period. Tab or the menu fast-forwards.
    bool running_ = true;
    bool fastForwardLatched_ = false;
    bool fastForwardActive_ = false;
    std::uint64_t lastTick_ = 0;
    double accumulator_ = 0;
    double coreMsAvg_ = 0;        // exponential average of core time per emulated frame
    double framesPerSecond_ = 0;  // emulated frames per real second, over the last half second
    std::uint64_t fpsFrames_ = 0;
    std::uint64_t fpsStart_ = 0;
    std::uint64_t emulatedFrames_ = 0;
    double coreMsTotal_ = 0;
    double peakFps_ = 0;

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
