#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>

#include "app/audio_output.hpp"
#include "app/debugger_ui.hpp"
#include "app/input.hpp"
#include "app/panels/panels.hpp"
#include "app/platform/file_dialog.hpp"
#include "app/platform/window.hpp"
#include "common/config.hpp"
#include "common/paths.hpp"
#include "common/wav.hpp"
#include "core/cart/rom.hpp"
#include "core/sched/machine.hpp"
#include "debug/debugger.hpp"

namespace gen::app {

inline constexpr const char* kSoundWindow = "Sound";
inline constexpr const char* kStatesWindow = "Save States";

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
    std::optional<std::filesystem::path> state;       // save state to load right after the ROM (M06-00a)
    bool debugger = false;                            // open the debugger panels in the debugger layout
    std::optional<std::uint64_t> pauseAt;             // pause once this frame is reached (M06-01)
    std::vector<std::uint32_t> breakAt;               // 68k exec breakpoints to set at start
    std::vector<std::string> focusWindows;            // windows brought to the front (screenshots)
    bool spriteBoxes = false;                         // outline sprites on the game view
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
    void handleHotkeys();
    void applyPadTypes();
    void flushSram();
    void drawSoundPanel();
    void toggleWavRecording();
    unsigned channelMask() const;  // writes dirty battery SRAM to <sha1>.srm
    std::string overlayText() const;

    // Save states (M06-00a): slots 1-9 live in <user data>/roms/<sha1>/states/slot<N>.gst.
    static constexpr int kStateSlots = 9;
    std::filesystem::path slotPath(int slot) const;
    bool saveState(const std::filesystem::path& path, const std::string& label);
    bool loadState(const std::filesystem::path& path, const std::string& label);
    void notify(std::string text);  // shown in the overlay for a few seconds
    // State browser (M06-08): slots and named states with thumbnails.
    struct StateEntry {
        std::filesystem::path path;
        std::string label;
        int slot = 0;  // 1-9, or 0 for a named state
        bool exists = false;
        bool valid = false;
        std::uint64_t frame = 0;
        std::uint64_t timestamp = 0;
        std::filesystem::file_time_type mtime{};
        std::unique_ptr<Texture> thumb;
    };
    std::filesystem::path statesDir() const;
    void scanStates();
    void drawStatesPanel();
    void pollStateDialogs();

    // Debugger (M06): commands from the panels / hotkeys, run between frames.
    void runDebugCommand(DebugCommand command);
    void onDebugStop(const RunResult& result);
    void drawStatusBar();
    void buildDebuggerLayout(unsigned dockspace);

    common::AppPaths paths_;
    common::ConfigFile config_;
    Window window_;
    RomFileDialog romDialog_;
    RomFileDialog stateOpenDialog_{RomFileDialog::Kind::State};
    RomFileDialog stateSaveDialog_{RomFileDialog::Kind::State};
    bool showStates_ = false;
    std::array<char, 64> stateName_{};
    std::vector<StateEntry> stateEntries_;
    std::uint64_t lastStateScan_ = 0;
    std::optional<cart::Rom> rom_;
    std::filesystem::path romPath_;
    std::optional<std::filesystem::path> lockOnPath_;
    std::unique_ptr<Machine> machine_;
    std::unique_ptr<debug::Debugger> debugger_;  // destroyed before machine_ (it owns its hooks)
    DebuggerUi debugUi_;
    bool debugLayout_ = false;  // build the debugger dock layout on the next frame
    GameView gameView_;
    InputManager input_;
    AudioOutput audio_;
    common::WavWriter wav_;
    std::filesystem::path wavPath_;
    std::array<bool, audio::kChannelCount> mute_{};
    std::array<bool, audio::kChannelCount> solo_{};
    float volume_ = 1.0f;
    bool showSound_ = false;
    std::string lastExport_;
    std::string machineSha1_;  // SHA-1 of the running image (lock-on: combined), keys the .srm file
    std::uint64_t lastSramCheck_ = 0;
    bool showInput_ = false;
    bool stepFrame_ = false;
    std::string notice_;
    std::uint64_t noticeUntil_ = 0;  // SDL performance counter
    bool stateLoadFailed_ = false;

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
