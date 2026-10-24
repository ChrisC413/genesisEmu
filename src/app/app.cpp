#include "app/app.hpp"

#include <algorithm>

#include <SDL3/SDL.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_internal.h>

#include "app/panels/panels.hpp"
#include "common/logging.hpp"
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
    if (options.rom && !openRom(*options.rom) && options.smokeFrames > 0) {
        log::shutdown();
        return kAppRomLoadFailed;
    }

    int frame = 0;
    while (!quit_) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL3_ProcessEvent(&event);
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
        if (window_.minimized() && options.smokeFrames == 0) {
            SDL_Delay(16);
            continue;
        }

        window_.beginFrame();
        if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_O, ImGuiInputFlags_RouteGlobal)) {
            romDialog_.open(window_.handle(),
                            romPath_.empty() ? std::nullopt : std::optional(romPath_.parent_path()));
        }
        drawDockSpace();
        drawMenuBar();
        drawGameView(&showGameView_, rom_);
        drawRomInfo(&showRomInfo_, rom_);
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

    const int result = options.smokeFrames > 0 ? smokeCheck(options) : kAppOk;
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
        ImGui::MenuItem("Save State", "Shift+F1", false, false);
        ImGui::MenuItem("Load State", "F1", false, false);
        ImGui::Separator();
        if (ImGui::MenuItem("Exit")) {
            quit_ = true;
        }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Emulation")) {
        ImGui::MenuItem("Run / Pause", "F5", false, false);
        ImGui::MenuItem("Reset", nullptr, false, false);
        if (ImGui::BeginMenu("Region", false)) {
            ImGui::EndMenu();
        }
        ImGui::Separator();
        ImGui::MenuItem(kGameViewWindow, nullptr, &showGameView_);
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Debug")) {
        ImGui::MenuItem("CPU Registers", nullptr, false, false);
        ImGui::MenuItem("Memory", nullptr, false, false);
        ImGui::MenuItem("Breakpoints", nullptr, false, false);
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
    for (const char* name : {kGameViewWindow, kRomInfoWindow}) {
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
    if (rom_) {
        logger().info("smoke test: ROM info shows '{}' sha1={}", rom_->header.overseasTitle, rom_->sha1Hex);
    }
    return ok ? kAppOk : kAppSmokeCheckFailed;
}

}  // namespace gen::app
