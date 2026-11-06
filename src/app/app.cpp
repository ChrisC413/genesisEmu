#include "app/app.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>
#include <format>
#include <fstream>
#include <iterator>

#include <SDL3/SDL.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_internal.h>

#include "app/panels/panels.hpp"
#include "common/logging.hpp"
#include "common/sram_file.hpp"
#include "core/state/save_state.hpp"
#include "core/state/state_hash.hpp"
#include "core/util/hash.hpp"
#include "core/version.hpp"

namespace gen::app {

namespace {

spdlog::logger& logger() {
    return log::get(log::Subsystem::App);
}

}  // namespace

App::App(common::AppPaths paths) : paths_(std::move(paths)) {}

int App::run(const AppOptions& options) {
    config_ = common::ConfigFile::loadOrCreate(paths_.configFile());
    for (const auto& warning : log::init(config_.config.log, {.console = true, .logDir = paths_.logDir()})) {
        logger().warn("{}", warning);
    }
    for (const auto& warning : config_.warnings) {
        log::get(log::Subsystem::Config).warn("{}", warning);
    }
    logger().info("genesisEmu {} starting; user data in '{}'", gen::version(), common::toUtf8(paths_.root()));

    const common::UiConfig& ui = config_.config.ui;
    std::string error;
    if (!window_.create({.title = "genesisEmu",
                         .logicalWidth = std::max(ui.windowWidth, 640),
                         .logicalHeight = std::max(ui.windowHeight, 400),
                         .maximized = ui.windowMaximized,
                         .vsync = true},
                        error)) {
        logger().error("{}", error);
        log::shutdown();
        return kAppNoGraphics;
    }
    logger().info("display content scale {:.2f}", window_.contentScale());

    if (!ui.dockLayout.empty()) {
        ImGui::LoadIniSettingsFromMemory(ui.dockLayout.c_str(), ui.dockLayout.size());
        layoutLoaded_ = true;
        layoutRestored_ = true;
    }
    input_.configure(config_.config.input);
    if (std::string audioError; !audio_.open(audioError)) {
        log::get(log::Subsystem::Audio).warn("{}; running without sound", audioError);
    }
    {
        auto& v = gameView_.settings();
        const auto& cv = config_.config.view;
        v.scale = cv.scale == "integer" ? GameViewSettings::Scale::Integer : GameViewSettings::Scale::Fit;
        v.aspectCorrect = cv.aspectCorrect;
        v.linearFilter = cv.linearFilter;
        v.showOverlay = cv.showOverlay;
    }
    lockOnPath_ = options.lockOn;
    debugUi_.setDefaultTracePath(common::toUtf8(paths_.root() / "traces"));
    debugUi_.setSpriteBoxes(options.spriteBoxes);
    if (options.debugger) {
        debugUi_.openAll();
        showStates_ = true;
        debugLayout_ = true;
    }
    fastForwardLatched_ = options.fastForward;
    if (options.rom && !openRom(*options.rom) && options.smokeFrames > 0) {
        log::shutdown();
        return kAppRomLoadFailed;
    }
    if (options.state) {
        if (!machine_) {
            logger().error("--state needs a ROM");
            stateLoadFailed_ = true;
        } else if (!loadState(*options.state, common::toUtf8(options.state->filename()))) {
            stateLoadFailed_ = true;
        }
        if (stateLoadFailed_ && options.smokeFrames > 0) {
            log::shutdown();
            return kAppRomLoadFailed;
        }
    }

    if (machine_) {
        for (const std::uint32_t addr : options.breakAt) {
            debugger_->toggleExec(CpuId::M68k, addr);
        }
    }

    int frame = 0;
    while (!quit_) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL3_ProcessEvent(&event);
            input_.handleEvent(event, !ImGui::GetIO().WantTextInput);
            switch (event.type) {
                case SDL_EVENT_QUIT:
                    quit_ = true;
                    break;
                case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                    if (event.window.windowID == SDL_GetWindowID(window_.handle())) {
                        quit_ = true;
                    }
                    break;
                case SDL_EVENT_DROP_FILE:
                    if (event.drop.data != nullptr) {
                        openRom(common::fromUtf8(event.drop.data));
                    }
                    break;
                default:
                    break;
            }
        }
        if (auto chosen = romDialog_.poll()) {
            openRom(*chosen);
        }
        pollStateDialogs();
        if (window_.minimized() && options.smokeFrames == 0) {
            SDL_Delay(16);
            continue;
        }

        handleHotkeys();
        if (machine_) {
            runDebugCommand(debugUi_.takeCommand());
        }
        stepEmulation(options.smokeFrames > 0);
        if (machine_ && options.pauseAt && running_ && machine_->frame() >= *options.pauseAt) {
            runDebugCommand(DebugCommand::Pause);
        }
        window_.beginFrame();
        fastForwardActive_ = input_.hotkeyHeld("fast-forward", !ImGui::GetIO().WantTextInput);
        if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_O, ImGuiInputFlags_RouteGlobal)) {
            romDialog_.open(window_.handle(),
                            romPath_.empty() ? std::nullopt : std::optional(romPath_.parent_path()));
        }
        drawDockSpace();
        drawMenuBar();
        drawStatusBar();
        std::vector<GameView::Box> boxes;
        for (const ScreenBox& b : debugUi_.overlayBoxes()) {
            boxes.push_back({b.x, b.y, b.w, b.h, b.colour});
        }
        gameView_.draw(&showGameView_, rom_, overlayText(), boxes);
        debugUi_.draw(running_);
        // One window per frame: focusing several at once only selects the last one's dock tab.
        if (frame >= 2 && static_cast<std::size_t>(frame - 2) < options.focusWindows.size()) {
            ImGui::SetWindowFocus(options.focusWindows[static_cast<std::size_t>(frame - 2)].c_str());
        }
        drawRomInfo(&showRomInfo_, rom_);
        drawStatesPanel();
        drawSoundPanel();
        if (input_.drawPanel(&showInput_)) {
            config_.config.input = input_.config();
            applyPadTypes();
            saveConfig();
        }
        drawPopups();
        const bool lastSmokeFrame = options.smokeFrames > 0 && frame + 1 >= options.smokeFrames;
        if (!window_.endFrame(lastSmokeFrame && options.screenshot ? &*options.screenshot : nullptr)) {
            logger().error("cannot save screenshot: {}", SDL_GetError());
            screenshotFailed_ = true;
        }

        if (ImGui::GetIO().WantSaveIniSettings) {
            persistLayout();
        }
        if (options.smokeFrames > 0 && ++frame >= options.smokeFrames) {
            quit_ = true;
        }
    }

    if (emulatedFrames_ != 0) {
        logger().info("emulated {} frames, core {:.3f} ms/frame on average, peak {:.0f} frames/s",
                      emulatedFrames_, coreMsTotal_ / static_cast<double>(emulatedFrames_), peakFps_);
    }
    const int result = options.smokeFrames > 0 ? smokeCheck(options) : kAppOk;
    flushSram();
    persistLayout();
    window_.logicalSize(config_.config.ui.windowWidth, config_.config.ui.windowHeight,
                        config_.config.ui.windowMaximized);
    saveConfig();
    logger().info("exiting with code {}", result);
    log::shutdown();
    return result;
}

bool App::openRom(const std::filesystem::path& path) {
    auto result = cart::loadRomFile(path);
    if (!result) {
        logger().error("{}", result.error);
        pendingError_ = result.error;
        return false;
    }
    rom_ = std::move(result.rom);
    romPath_ = path;
    for (const auto& warning : rom_->warnings) {
        logger().warn("{}: {}", common::toUtf8(path.filename()), warning);
    }
    logger().info("loaded '{}' sha1={} ({})", common::toUtf8(path), rom_->sha1Hex,
                  rom_->known ? std::string(rom_->known->title) + " " + std::string(rom_->known->revision)
                              : std::string("unknown ROM"));
    showRomInfo_ = true;
    addRecentRom(path);
    startMachine();
    saveConfig();
    return true;
}

void App::addRecentRom(const std::filesystem::path& path) {
    std::error_code ec;
    const auto absolute = std::filesystem::absolute(path, ec);
    const std::string entry = common::toUtf8(ec ? path : absolute);
    auto& recent = config_.config.ui.recentRoms;
    std::erase(recent, entry);
    recent.insert(recent.begin(), entry);
    if (recent.size() > config_.config.ui.maxRecentRoms) {
        recent.resize(config_.config.ui.maxRecentRoms);
    }
}

void App::drawDockSpace() {
    const ImGuiID dockspace = ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());
    if (debugLayout_) {
        buildDebuggerLayout(dockspace);
        debugLayout_ = false;
        layoutLoaded_ = true;
        return;
    }
    if (layoutLoaded_ && !resetLayout_) {
        return;
    }
    // First run (or Help > Reset Layout): game view in the centre, ROM info on the right.
    ImGui::DockBuilderRemoveNode(dockspace);
    ImGui::DockBuilderAddNode(dockspace, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspace, ImGui::GetMainViewport()->WorkSize);
    ImGuiID centre = dockspace;
    const ImGuiID right = ImGui::DockBuilderSplitNode(centre, ImGuiDir_Right, 0.34f, nullptr, &centre);
    ImGui::DockBuilderDockWindow(kGameViewWindow, centre);
    ImGui::DockBuilderDockWindow(kRomInfoWindow, right);
    ImGui::DockBuilderFinish(dockspace);
    showGameView_ = true;
    showRomInfo_ = true;
    layoutLoaded_ = true;
    resetLayout_ = false;
}

void App::drawMenuBar() {
    if (!ImGui::BeginMainMenuBar()) {
        return;
    }
    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("Open ROM...", "Ctrl+O")) {
            romDialog_.open(window_.handle(),
                            romPath_.empty() ? std::nullopt : std::optional(romPath_.parent_path()));
        }
        auto& recent = config_.config.ui.recentRoms;
        if (ImGui::BeginMenu("Recent ROMs", !recent.empty())) {
            std::optional<std::string> chosen;
            for (const auto& entry : recent) {
                if (ImGui::MenuItem(entry.c_str())) {
                    chosen = entry;
                }
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Clear list")) {
                recent.clear();
                saveConfig();
            }
            ImGui::EndMenu();
            if (chosen) {
                openRom(common::fromUtf8(*chosen));
            }
        }
        ImGui::Separator();
        ImGui::MenuItem("Open Project...", nullptr, false, false);
        ImGui::MenuItem("Save Project", nullptr, false, false);
        ImGui::Separator();
        const auto& hotkeys = input_.config().hotkeys;
        const auto shortcut = [&](const std::string& action) {
            const auto it = hotkeys.find(action);
            return it != hotkeys.end() ? it->second.c_str() : nullptr;
        };
        if (ImGui::BeginMenu("Save State", machine_ != nullptr)) {
            for (int slot = 1; slot <= kStateSlots; ++slot) {
                if (ImGui::MenuItem(std::format("Slot {}", slot).c_str(),
                                    shortcut(std::format("save-state-{}", slot)))) {
                    saveState(slotPath(slot), std::format("slot {}", slot));
                }
            }
            ImGui::EndMenu();
        }
        if (ImGui::MenuItem("Save State As...", nullptr, false, machine_ != nullptr)) {
            stateSaveDialog_.open(window_.handle(), statesDir(), true);
        }
        if (ImGui::MenuItem("Load State File...", nullptr, false, machine_ != nullptr)) {
            stateOpenDialog_.open(window_.handle(), statesDir());
        }
        if (ImGui::MenuItem("Save States...", nullptr, &showStates_, machine_ != nullptr)) {
            scanStates();
        }
        if (ImGui::BeginMenu("Load State", machine_ != nullptr)) {
            for (int slot = 1; slot <= kStateSlots; ++slot) {
                std::error_code ec;
                const bool exists = std::filesystem::exists(slotPath(slot), ec);
                const std::string label =
                    exists ? std::format("Slot {}", slot) : std::format("Slot {} (empty)", slot);
                if (ImGui::MenuItem(label.c_str(), shortcut(std::format("load-state-{}", slot)), false,
                                    exists)) {
                    loadState(slotPath(slot), std::format("slot {}", slot));
                }
            }
            ImGui::EndMenu();
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Exit")) {
            quit_ = true;
        }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Emulation")) {
        if (ImGui::MenuItem(running_ ? "Pause" : "Run", "F5", false, machine_ != nullptr)) {
            debugUi_.request(running_ ? DebugCommand::Pause : DebugCommand::Run);
        }
        if (ImGui::MenuItem("Soft Reset", nullptr, false, machine_ != nullptr)) {
            debugUi_.request(DebugCommand::SoftReset);
        }
        if (ImGui::MenuItem("Hard Reset", nullptr, false, machine_ != nullptr)) {
            debugUi_.request(DebugCommand::HardReset);
        }
        ImGui::MenuItem("Fast-forward", "hold Tab", &fastForwardLatched_, machine_ != nullptr);
        const bool canStep = machine_ != nullptr && !running_;
        if (ImGui::MenuItem("Step Frame", "F6", false, canStep)) {
            stepFrame_ = true;
        }
        if (ImGui::MenuItem("Step Scanline", "Shift+F6", false, canStep)) {
            debugUi_.request(DebugCommand::StepLine);
        }
        if (ImGui::MenuItem("Step Into", "F11", false, canStep)) {
            debugUi_.request(DebugCommand::StepInto);
        }
        if (ImGui::MenuItem("Step Over", "F10", false, canStep)) {
            debugUi_.request(DebugCommand::StepOver);
        }
        if (ImGui::MenuItem("Step Out", "Shift+F11", false, canStep)) {
            debugUi_.request(DebugCommand::StepOut);
        }
        if (ImGui::BeginMenu("Focused CPU")) {
            if (ImGui::MenuItem("68000", nullptr, debugUi_.focus() == CpuId::M68k)) {
                debugUi_.setFocus(CpuId::M68k);
            }
            if (ImGui::MenuItem("Z80", nullptr, debugUi_.focus() == CpuId::Z80)) {
                debugUi_.setFocus(CpuId::Z80);
            }
            ImGui::EndMenu();
        }
        ImGui::Separator();
        ImGui::MenuItem("Input...", nullptr, &showInput_);
        ImGui::MenuItem("Sound...", nullptr, &showSound_);
        if (ImGui::BeginMenu("Region", false)) {
            ImGui::EndMenu();
        }
        ImGui::Separator();
        ImGui::MenuItem(kGameViewWindow, nullptr, &showGameView_);
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Debug")) {
        if (ImGui::MenuItem("Debugger Layout")) {
            debugUi_.openAll();
            debugLayout_ = true;
        }
        ImGui::Separator();
        debugUi_.drawMenu();
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Analysis")) {
        ImGui::MenuItem(kRomInfoWindow, nullptr, &showRomInfo_);
        ImGui::MenuItem("ROM Map", nullptr, false, false);
        ImGui::MenuItem("Disassembly", nullptr, false, false);
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Help")) {
        if (ImGui::MenuItem("Reset Layout")) {
            resetLayout_ = true;
        }
        if (ImGui::MenuItem("About genesisEmu")) {
            openAbout_ = true;
        }
        ImGui::EndMenu();
    }
    ImGui::EndMainMenuBar();
}

void App::drawPopups() {
    if (openAbout_) {
        ImGui::OpenPopup("About genesisEmu");
        openAbout_ = false;
    }
    if (ImGui::BeginPopupModal("About genesisEmu", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("genesisEmu %.*s", static_cast<int>(gen::version().size()), gen::version().data());
        ImGui::TextUnformatted("Sega Genesis emulator and reverse-engineering workbench.");
        ImGui::TextUnformatted("Copyright (c) 2026 Chris Cowin. MIT License.");
        ImGui::Text("Dear ImGui %s", ImGui::GetVersion());
        ImGui::TextDisabled("User data: %s", common::toUtf8(paths_.root()).c_str());
        if (ImGui::Button("Close")) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (!pendingError_.empty()) {
        ImGui::OpenPopup("Cannot open ROM");
    }
    if (ImGui::BeginPopupModal("Cannot open ROM", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted(pendingError_.c_str());
        if (ImGui::Button("OK")) {
            pendingError_.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void App::persistLayout() {
    std::size_t size = 0;
    const char* ini = ImGui::SaveIniSettingsToMemory(&size);
    config_.config.ui.dockLayout.assign(ini, size);
    ImGui::GetIO().WantSaveIniSettings = false;
}

void App::saveConfig() {
    const auto& v = gameView_.settings();
    auto& cv = config_.config.view;
    cv.scale = v.scale == GameViewSettings::Scale::Integer ? "integer" : "fit";
    cv.aspectCorrect = v.aspectCorrect;
    cv.linearFilter = v.linearFilter;
    cv.showOverlay = v.showOverlay;
    if (!config_.save()) {
        log::get(log::Subsystem::Config).error("{}", config_.warnings.back());
    }
}

int App::smokeCheck(const AppOptions& options) const {
    bool ok = true;
    const auto fail = [&ok](const char* what) {
        logger().error("smoke test: {}", what);
        ok = false;
    };
    for (const auto& [name, shown] :
         {std::pair(kGameViewWindow, showGameView_), std::pair(kRomInfoWindow, showRomInfo_)}) {
        if (!shown) {
            continue;  // closed by the layout (the debugger layout closes ROM Info)
        }
        const ImGuiWindow* w = ImGui::FindWindowByName(name);
        if (w == nullptr || w->DockId == 0) {
            fail("panel is not docked");
        }
    }
    if (options.expectRestoredLayout && !layoutRestored_) {
        fail("no saved layout");
    }
    if (screenshotFailed_) {
        fail("screenshot not saved");
    }
    if (options.rom && !rom_) {
        fail("ROM not loaded");
    }
    if (options.state && stateLoadFailed_) {
        fail("save state not loaded");
    }
    if (machine_ && !options.breakAt.empty()) {
        const debug::StopInfo& stop = debugger_->lastStop();
        if (running_ || stop.kind != debug::StopInfo::Kind::Breakpoint ||
            std::find(options.breakAt.begin(), options.breakAt.end(), machine_->cpu().state().pc) ==
                options.breakAt.end()) {
            fail("the execution breakpoint did not stop the emulation");
        } else {
            logger().info("smoke test: {}", stop.text);
        }
    }
    if (machine_ && options.pauseAt && options.breakAt.empty() &&
        (running_ || machine_->frame() != *options.pauseAt)) {
        fail("did not pause at the requested frame");
    }
    if (rom_) {
        logger().info("smoke test: ROM info shows '{}' sha1={}", rom_->header.overseasTitle, rom_->sha1Hex);
        if (!machine_ || (machine_->frame() == 0 && machine_->line() == 0)) {
            fail("emulation did not run");
        } else if (!gameView_.hasFrame()) {
            fail("no frame reached the game view");
        } else {
            logger().info("smoke test: {} frames emulated, {} uploaded, frame hash {}", machine_->frame(),
                          gameView_.uploads(), state::hashToHex(machine_->vdp().frameHash()));
        }
    }
    return ok ? kAppOk : kAppSmokeCheckFailed;
}

void App::startMachine() {
    std::vector<std::uint8_t> image = rom_->data;
    std::uint8_t regions = rom_->header.regions;
    if (lockOnPath_) {
        if (auto locked = cart::loadRomFile(*lockOnPath_)) {
            image = cart::lockOnImage(rom_->data, locked.rom->data);
            regions = locked.rom->header.regions;
            logger().info("locked on '{}'", common::toUtf8(*lockOnPath_));
        } else {
            logger().error("lock-on: {}", locked.error);
        }
    }
    flushSram();  // the previous game's battery save
    machineSha1_ = toHex(sha1(image));
    debugUi_.attach(nullptr, nullptr);
    debugger_.reset();
    machine_ = std::make_unique<Machine>(std::move(image), regions);
    debugger_ = std::make_unique<debug::Debugger>(*machine_);
    debugUi_.attach(machine_.get(), debugger_.get());
    applyPadTypes();
    audio_.setSourceRate(machine_->sound().sampleRate());
    if (wav_.isOpen()) {
        wav_.close();
    }
    if (cart::SramDevice* sram = machine_->sram()) {
        const auto path = common::sramPath(paths_, machineSha1_);
        if (common::loadSram(*sram, path)) {
            logger().info("battery save loaded from '{}'", common::toUtf8(path));
        }
    }
    gameView_.clear();
    running_ = true;
    accumulator_ = 0;
}

void App::stepEmulation(bool smoke) {
    const std::uint64_t now = SDL_GetPerformanceCounter();
    const auto freq = static_cast<double>(SDL_GetPerformanceFrequency());
    const double dt = lastTick_ != 0 ? static_cast<double>(now - lastTick_) / freq : 0.0;
    lastTick_ = now;
    if (fpsStart_ == 0) {
        fpsStart_ = now;
    } else if (static_cast<double>(now - fpsStart_) / freq >= 0.5) {
        framesPerSecond_ = static_cast<double>(fpsFrames_) / (static_cast<double>(now - fpsStart_) / freq);
        peakFps_ = std::max(peakFps_, framesPerSecond_);
        fpsFrames_ = 0;
        fpsStart_ = now;
    }
    if (machine_ && !running_ && stepFrame_) {
        machine_->setInput(0, input_.buttons(0, true));
        machine_->setInput(1, input_.buttons(1, true));
        onDebugStop(debugger_->run(RunLimit::frames(1)));
        machine_->sound().clearSamples();  // a single stepped frame is not played
        gameView_.upload(machine_->vdp());
    }
    stepFrame_ = false;
    if (machine_ && now - lastSramCheck_ > static_cast<std::uint64_t>(freq * 2)) {
        lastSramCheck_ = now;
        flushSram();  // periodically while dirty (spec 04)
    }
    audio_.setPaused(!machine_ || !running_);
    audio_.setMuted(fastForwardActive_ || fastForwardLatched_);  // spec 06: mute while fast-forwarding
    if (!machine_ || !running_) {
        accumulator_ = 0;
        return;
    }
    const double period = machine_->pal() ? 1.0 / 49.70 : 1.0 / 59.92;
    const auto elapsed = [&] { return static_cast<double>(SDL_GetPerformanceCounter() - now) / freq; };
    int frames = 0;
    const bool keyboardFree = !ImGui::GetIO().WantTextInput;
    const auto runFrame = [&] {
        machine_->setInput(0, input_.buttons(0, keyboardFree));
        machine_->setInput(1, input_.buttons(1, keyboardFree));
        machine_->sound().setChannelMask(channelMask());
        const RunResult r = debugger_->run(RunLimit::frames(1));
        audio_.push(machine_->sound().samples());
        wav_.write(machine_->sound().samples());
        machine_->sound().clearSamples();
        ++frames;
        if (r.reason != StopReason::LimitReached) {
            onDebugStop(r);  // a breakpoint / watchpoint / finished step: pause mid-frame
        }
    };
    if (fastForwardActive_ || fastForwardLatched_) {
        do {  // unthrottled: as many frames as fit in most of a 60 Hz tick
            runFrame();
        } while (running_ && elapsed() < 0.012 && frames < 1000);
        accumulator_ = 0;
    } else if (smoke) {
        runFrame();
    } else {
        // Host vsync drives the video when the display refresh is within 1 Hz of the console's; the small
        // rate difference is left to audio rate control (M05). Otherwise pace against the real clock.
        float refresh = 0;
        if (const SDL_DisplayMode* mode =
                SDL_GetCurrentDisplayMode(SDL_GetDisplayForWindow(window_.handle()))) {
            refresh = mode->refresh_rate;
        }
        if (std::abs(static_cast<double>(refresh) - 1.0 / period) < 1.0) {
            runFrame();
            accumulator_ = 0;
        } else {
            accumulator_ = std::min(accumulator_ + dt, 0.25);
            while (running_ && accumulator_ >= period && frames < 4) {
                runFrame();
                accumulator_ -= period;
            }
        }
    }
    if (frames == 0) {
        return;
    }
    const double ms = elapsed() * 1000.0 / frames;
    coreMsAvg_ = coreMsAvg_ == 0 ? ms : coreMsAvg_ * 0.95 + ms * 0.05;
    coreMsTotal_ += ms * frames;
    emulatedFrames_ += static_cast<std::uint64_t>(frames);
    fpsFrames_ += static_cast<std::uint64_t>(frames);
    gameView_.upload(machine_->vdp());
}

std::string App::overlayText() const {
    if (!machine_) {
        return {};
    }
    std::string text = std::format("{:.1f} fps | core {:.2f} ms/frame", framesPerSecond_, coreMsAvg_);
    if (!running_) {
        text += " | paused";
    } else if (fastForwardActive_ || fastForwardLatched_) {
        text += std::format(" | fast-forward {:.1f}x", framesPerSecond_ / 59.92);
    }
    if (!notice_.empty() && SDL_GetPerformanceCounter() < noticeUntil_) {
        text += " | " + notice_;
    }
    return text;
}

void App::handleHotkeys() {
    for (const std::string& action : input_.takeHotkeys()) {
        int slot = 0;
        const auto slotOf = [&](std::string_view prefix) {
            if (!action.starts_with(prefix) || action.size() != prefix.size() + 1) {
                return false;
            }
            slot = action.back() - '0';
            return slot >= 1 && slot <= kStateSlots;
        };
        if (action == "pause") {
            debugUi_.request(running_ ? DebugCommand::Pause : DebugCommand::Run);
        } else if (action == "step-frame") {
            stepFrame_ = !running_;
        } else if (action == "step-scanline" || action == "step-instruction" || action == "step-over" ||
                   action == "step-out") {
            if (!running_) {
                debugUi_.request(action == "step-scanline"      ? DebugCommand::StepLine
                                 : action == "step-instruction" ? DebugCommand::StepInto
                                 : action == "step-over"        ? DebugCommand::StepOver
                                                                : DebugCommand::StepOut);
            }
        } else if (slotOf("save-state-")) {
            if (machine_) {
                saveState(slotPath(slot), std::format("slot {}", slot));
            }
        } else if (slotOf("load-state-")) {
            if (machine_) {
                loadState(slotPath(slot), std::format("slot {}", slot));
            }
        } else {
            logger().info("hotkey '{}' is not available yet", action);
        }
    }
}

std::filesystem::path App::slotPath(int slot) const {
    return paths_.romDataDir(machineSha1_) / "states" /
           std::format("slot{}{}", slot, std::string_view(state::kStateExtension));
}

void App::notify(std::string text) {
    notice_ = std::move(text);
    noticeUntil_ = SDL_GetPerformanceCounter() + SDL_GetPerformanceFrequency() * 3;
}

// Called between frames, so the machine is at an instruction boundary (spec 08).
bool App::saveState(const std::filesystem::path& path, const std::string& label) {
    const auto bytes = machine_->saveState(static_cast<std::uint64_t>(std::time(nullptr)), true);
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    std::ofstream f(path, std::ios::binary);
    f.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!f) {
        logger().error("cannot write save state '{}'", common::toUtf8(path));
        notify("cannot save " + label);
        return false;
    }
    logger().info("saved frame {} to '{}'", machine_->frame(), common::toUtf8(path));
    notify("saved " + label);
    return true;
}

bool App::loadState(const std::filesystem::path& path, const std::string& label) {
    std::ifstream in(path, std::ios::binary);
    const std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(in)),
                                          std::istreambuf_iterator<char>());
    std::string error = bytes.empty() ? "the file is missing or empty" : "";
    if (!error.empty() || !machine_->loadState(bytes, error)) {
        logger().error("cannot load save state '{}': {}", common::toUtf8(path), error);
        notify("cannot load " + label + ": " + error);
        return false;
    }
    logger().info("loaded '{}' (frame {})", common::toUtf8(path), machine_->frame());
    notify(std::format("loaded {} (frame {})", label, machine_->frame()));
    audio_.flush();
    accumulator_ = 0;
    gameView_.upload(machine_->vdp());
    debugUi_.attach(machine_.get(), debugger_.get());  // new baseline for "changed since the last stop"
    return true;
}

void App::applyPadTypes() {
    if (machine_) {
        machine_->setPadType(0, input_.padType(0));
        machine_->setPadType(1, input_.padType(1));
    }
}

void App::flushSram() {
    cart::SramDevice* sram = machine_ ? machine_->sram() : nullptr;
    if (sram == nullptr || !sram->dirty() || machineSha1_.empty()) {
        return;
    }
    const auto path = common::sramPath(paths_, machineSha1_);
    if (common::saveSram(*sram, path)) {
        logger().info("battery save written to '{}'", common::toUtf8(path));
    } else {
        logger().error("cannot write battery save '{}'", common::toUtf8(path));
    }
}

// ---- Debugger (M06) -----------------------------------------------------------------------------------

void App::onDebugStop(const RunResult& result) {
    if (result.reason == StopReason::LimitReached) {
        debugUi_.onStop();
        return;
    }
    running_ = false;
    accumulator_ = 0;
    const debug::StopInfo& stop = debugger_->lastStop();
    logger().info("stopped at frame {} line {}: {}", machine_->frame(), machine_->line(),
                  stop.text.empty() ? std::string(toString(result.reason)) : stop.text);
    debugUi_.onStop();
    gameView_.upload(machine_->vdp());
}

void App::runDebugCommand(DebugCommand command) {
    const CpuId cpu = debugUi_.focus();
    const auto stepped = [&] {
        machine_->sound().clearSamples();  // stepping is not played
        running_ = false;
        debugUi_.onStop();
        gameView_.upload(machine_->vdp());
    };
    switch (command) {
        case DebugCommand::None:
            return;
        case DebugCommand::Run:
            running_ = true;
            return;
        case DebugCommand::Pause:
            running_ = false;
            debugger_->cancelStep();
            debugger_->notePause();
            debugUi_.onStop();
            return;
        case DebugCommand::StepInto:
            debugger_->stepInto(cpu);
            stepped();
            return;
        case DebugCommand::StepOver:
            if (debugger_->beginStepOver(cpu)) {
                running_ = true;  // runs (and plays) until the call returns or something breaks
            } else {
                stepped();
            }
            return;
        case DebugCommand::StepOut:
            debugger_->beginStepOut(cpu);
            running_ = true;
            return;
        case DebugCommand::RunTo:
            debugger_->beginRunTo(cpu, debugUi_.runToAddress());
            running_ = true;
            return;
        case DebugCommand::StepLine:
            debugger_->run(RunLimit::lines(1));
            stepped();
            return;
        case DebugCommand::StepFrame:
            stepFrame_ = true;
            return;
        case DebugCommand::SoftReset:
            machine_->softReset();
            debugUi_.onStop();
            notify("soft reset");
            return;
        case DebugCommand::HardReset:
            machine_->hardReset();
            accumulator_ = 0;
            debugUi_.onStop();
            notify("hard reset");
            return;
    }
}

void App::drawStatusBar() {
    const float height = ImGui::GetFrameHeight();
    if (ImGui::BeginViewportSideBar(
            "##status", ImGui::GetMainViewport(), ImGuiDir_Down, height,
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_MenuBar)) {
        if (ImGui::BeginMenuBar()) {
            if (machine_) {
                const bool stepping = debugger_->stepPending();
                ImGui::TextColored(running_ ? ImVec4(0.5f, 0.9f, 0.5f, 1) : ImVec4(1, 0.8f, 0.3f, 1), "%s",
                                   running_ ? (stepping ? "STEPPING" : "RUNNING") : "PAUSED");
                ImGui::Text("| %s | frame %llu  line %3d  MCLK %llu | 68k $%06X  Z80 $%04X",
                            debugUi_.focus() == CpuId::Z80 ? "Z80" : "68k",
                            static_cast<unsigned long long>(machine_->frame()), machine_->line(),
                            static_cast<unsigned long long>(machine_->cpuMclk()), machine_->cpu().state().pc,
                            machine_->z80Cpu().state().pc);
                if (!running_ && !debugger_->lastStop().text.empty()) {
                    ImGui::Text("| %s", debugger_->lastStop().text.c_str());
                }
            } else {
                ImGui::TextDisabled("no ROM");
            }
            ImGui::EndMenuBar();
        }
    }
    ImGui::End();
}

void App::buildDebuggerLayout(unsigned dockspace) {
    // Game view top left; code and registers on the right; memory, breakpoints and viewers at the bottom.
    ImGui::DockBuilderRemoveNode(dockspace);
    ImGui::DockBuilderAddNode(dockspace, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspace, ImGui::GetMainViewport()->WorkSize);
    ImGuiID main = dockspace;
    ImGuiID bottom = ImGui::DockBuilderSplitNode(main, ImGuiDir_Down, 0.42f, nullptr, &main);
    ImGuiID right = ImGui::DockBuilderSplitNode(main, ImGuiDir_Right, 0.58f, nullptr, &main);
    ImGuiID regs = ImGui::DockBuilderSplitNode(right, ImGuiDir_Right, 0.45f, nullptr, &right);
    const ImGuiID control = ImGui::DockBuilderSplitNode(regs, ImGuiDir_Up, 0.32f, nullptr, &regs);
    const ImGuiID bottomRight = ImGui::DockBuilderSplitNode(bottom, ImGuiDir_Right, 0.45f, nullptr, &bottom);
    ImGui::DockBuilderDockWindow(kGameViewWindow, main);
    ImGui::DockBuilderDockWindow(kRomInfoWindow, right);
    ImGui::DockBuilderDockWindow(kTraceWindow, right);
    ImGui::DockBuilderDockWindow(kCallStackWindow, right);
    ImGui::DockBuilderDockWindow(kDisassemblyWindow, right);
    ImGui::DockBuilderDockWindow(kDebuggerWindow, control);
    ImGui::DockBuilderDockWindow(kVdpRegsWindow, regs);
    ImGui::DockBuilderDockWindow(kZ80RegsWindow, regs);
    ImGui::DockBuilderDockWindow(kM68kRegsWindow, regs);
    ImGui::DockBuilderDockWindow(kBreakpointsWindow, bottom);
    for (int i = kMemoryViews; i >= 1; --i) {
        ImGui::DockBuilderDockWindow(std::format("Memory {}", i).c_str(), bottom);
    }
    ImGui::DockBuilderDockWindow(kSoundWindow, bottomRight);
    ImGui::DockBuilderDockWindow(kStatesWindow, bottomRight);
    ImGui::DockBuilderDockWindow(kPaletteWindow, bottomRight);
    ImGui::DockBuilderDockWindow(kSpritesWindow, bottomRight);
    ImGui::DockBuilderDockWindow(kPlanesWindow, bottomRight);
    ImGui::DockBuilderDockWindow(kTilesWindow, bottomRight);
    ImGui::DockBuilderFinish(dockspace);
    showGameView_ = true;
    showRomInfo_ = false;  // docked behind the disassembly when reopened
}

// ---- Save state browser (M06-08) ------------------------------------------------------------------------

std::filesystem::path App::statesDir() const {
    return paths_.romDataDir(machineSha1_) / "states";
}

void App::scanStates() {
    lastStateScan_ = SDL_GetPerformanceCounter();
    std::vector<StateEntry> found;
    std::error_code ec;
    const auto dir = statesDir();
    for (int slot = 1; slot <= kStateSlots; ++slot) {
        StateEntry e;
        e.path = slotPath(slot);
        e.label = std::format("Slot {}", slot);
        e.slot = slot;
        found.push_back(std::move(e));
    }
    if (std::filesystem::is_directory(dir, ec)) {
        std::vector<std::filesystem::path> named;
        for (const auto& f : std::filesystem::directory_iterator(dir, ec)) {
            const auto name = f.path().filename().string();
            if (f.path().extension() == state::kStateExtension &&
                !(name.starts_with("slot") && name.size() == 9)) {
                named.push_back(f.path());
            }
        }
        std::sort(named.begin(), named.end());
        for (const auto& p : named) {
            StateEntry e;
            e.path = p;
            e.label = common::toUtf8(p.stem());
            found.push_back(std::move(e));
        }
    }
    for (StateEntry& e : found) {
        e.exists = std::filesystem::exists(e.path, ec);
        if (!e.exists) {
            continue;
        }
        e.mtime = std::filesystem::last_write_time(e.path, ec);
        // Keep the decoded picture of an unchanged file.
        const auto old = std::find_if(stateEntries_.begin(), stateEntries_.end(), [&](const StateEntry& o) {
            return o.path == e.path && o.mtime == e.mtime;
        });
        if (old != stateEntries_.end()) {
            e.thumb = std::move(old->thumb);
            e.frame = old->frame;
            e.timestamp = old->timestamp;
            e.valid = old->valid;
            continue;
        }
        std::ifstream in(e.path, std::ios::binary);
        const std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(in)),
                                              std::istreambuf_iterator<char>());
        std::string error;
        if (const auto info = state::peekStateInfo(bytes, error)) {
            e.valid = true;
            e.frame = info->frame;
            e.timestamp = info->timestamp;
        }
        if (const auto t = state::peekThumbnail(bytes)) {
            e.thumb = std::make_unique<Texture>();
            e.thumb->update(t->width, t->height, t->rgba.data());
        }
    }
    stateEntries_ = std::move(found);
}

void App::drawStatesPanel() {
    if (!showStates_) {
        return;
    }
    if (!ImGui::Begin(kStatesWindow, &showStates_)) {
        ImGui::End();
        return;
    }
    if (!machine_) {
        ImGui::TextDisabled("Open a ROM first.");
        ImGui::End();
        return;
    }
    if (static_cast<double>(SDL_GetPerformanceCounter() - lastStateScan_) >
        static_cast<double>(SDL_GetPerformanceFrequency()) * 2.0) {
        scanStates();
    }
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 12);
    const bool enter = ImGui::InputTextWithHint("##name", "name of a new state", stateName_.data(),
                                                stateName_.size(), ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::SameLine();
    if ((ImGui::Button("Save named") || enter) && stateName_[0] != 0) {
        std::string name = stateName_.data();
        for (char& c : name) {
            if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' ||
                c == '>' || c == '|') {
                c = '_';
            }
        }
        saveState(statesDir() / (name + std::string(state::kStateExtension)), name);
        stateName_[0] = 0;
        scanStates();
    }
    ImGui::SameLine();
    if (ImGui::Button("Save as...")) {
        stateSaveDialog_.open(window_.handle(), statesDir(), true);
    }
    ImGui::SameLine();
    if (ImGui::Button("Load file...")) {
        stateOpenDialog_.open(window_.handle(), statesDir());
    }
    ImGui::SameLine();
    if (ImGui::Button("Refresh")) {
        scanStates();
    }
    ImGui::TextDisabled("States are saved between frames or while paused, i.e. at an instruction boundary.");
    const float thumbW = state::kThumbnailWidth * 0.75f;
    const float thumbH = state::kThumbnailHeight * 0.75f;
    std::optional<std::filesystem::path> toLoad;
    std::optional<std::pair<std::filesystem::path, std::string>> toSave;
    std::optional<std::filesystem::path> toDelete;
    if (ImGui::BeginTable("states", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY)) {
        ImGui::TableSetupColumn("picture", ImGuiTableColumnFlags_WidthFixed, thumbW);
        ImGui::TableSetupColumn("state");
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed);
        for (const StateEntry& e : stateEntries_) {
            ImGui::PushID(e.label.c_str());
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            if (e.thumb) {
                ImGui::Image(static_cast<ImTextureID>(e.thumb->id()), ImVec2(thumbW, thumbH));
            } else {
                ImGui::Dummy(ImVec2(thumbW, e.exists ? thumbH : ImGui::GetTextLineHeight()));
            }
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(e.label.c_str());
            if (!e.exists) {
                ImGui::TextDisabled("empty");
            } else if (!e.valid) {
                ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "not a readable state");
            } else {
                ImGui::Text("frame %llu", static_cast<unsigned long long>(e.frame));
                if (e.timestamp != 0) {
                    // Calendar arithmetic only (no chrono formatters, which older libc++ lacks).
                    const auto secs = static_cast<std::int64_t>(e.timestamp);
                    const std::chrono::sys_days day{std::chrono::days{secs / 86400}};
                    const std::chrono::year_month_day ymd{day};
                    const std::int64_t rest = secs % 86400;
                    ImGui::TextDisabled("%04d-%02u-%02u %02d:%02d:%02d UTC", static_cast<int>(ymd.year()),
                                        static_cast<unsigned>(ymd.month()), static_cast<unsigned>(ymd.day()),
                                        static_cast<int>(rest / 3600), static_cast<int>(rest / 60 % 60),
                                        static_cast<int>(rest % 60));
                }
            }
            ImGui::TableNextColumn();
            ImGui::BeginDisabled(!e.exists || !e.valid);
            if (ImGui::Button("Load")) {
                toLoad = e.path;
            }
            ImGui::EndDisabled();
            if (e.slot != 0 || e.exists) {
                if (ImGui::Button(e.exists ? "Overwrite" : "Save")) {
                    toSave = std::pair(e.path, e.label);
                }
            }
            if (e.exists && ImGui::Button("Delete")) {
                toDelete = e.path;
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    ImGui::End();
    if (toLoad) {
        loadState(*toLoad, common::toUtf8(toLoad->stem()));
    }
    if (toSave) {
        saveState(toSave->first, toSave->second);
        scanStates();
    }
    if (toDelete) {
        std::error_code ec;
        std::filesystem::remove(*toDelete, ec);
        notify("deleted " + common::toUtf8(toDelete->stem()));
        scanStates();
    }
}

void App::pollStateDialogs() {
    if (auto chosen = stateOpenDialog_.poll(); chosen && machine_) {
        loadState(*chosen, common::toUtf8(chosen->filename()));
    }
    if (auto chosen = stateSaveDialog_.poll(); chosen && machine_) {
        if (!chosen->has_extension()) {
            chosen->replace_extension(state::kStateExtension);
        }
        saveState(*chosen, common::toUtf8(chosen->filename()));
        scanStates();
    }
}

}  // namespace gen::app
