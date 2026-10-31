#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace gen::common {

inline constexpr int kConfigSchemaVersion = 1;

struct LogConfig {
    std::string level = "info";                     // default for every subsystem
    std::map<std::string, std::string> subsystems;  // per-subsystem overrides, e.g. {"vdp": "debug"}
    bool toFile = true;
    std::uint32_t maxFileSizeKb = 5 * 1024;
    std::uint32_t maxFiles = 3;
};

struct UiConfig {
    int windowWidth = 1280;  // logical pixels
    int windowHeight = 800;
    bool windowMaximized = false;
    std::string dockLayout;  // Dear ImGui .ini settings blob
    std::vector<std::string> recentRoms;
    std::size_t maxRecentRoms = 10;
};

// Game view presentation (M03-10 settings, persisted since M04-02).
struct ViewConfig {
    std::string scale = "fit";  // "fit" | "integer"
    bool aspectCorrect = true;
    bool linearFilter = false;
    bool showOverlay = true;
};

// Controller mapping (spec 07 "Input", M04-02). Bindings map a pad button name (UP DOWN LEFT RIGHT A B C
// START X Y Z MODE) to an SDL key name (SDL_GetScancodeName) or an SDL gamepad button name
// (SDL_GetGamepadStringForButton). Hotkeys map an action name to a key name ("Shift+F1" for modifiers).
struct InputConfig {
    std::array<std::string, 2> padType{"3", "3"};  // "3", "6" or "none" per port
    std::array<std::map<std::string, std::string>, 2> keys;
    std::map<std::string, std::string> gamepad;  // applied to the first gamepad on port 1, second on port 2
    std::map<std::string, std::string> hotkeys;
};

// Spec 07 defaults: arrows = D-pad, A/S/D = A/B/C, Q/W/E = X/Y/Z, Enter = Start, Right Shift = Mode.
InputConfig defaultInputConfig();

struct Config {
    LogConfig log;
    UiConfig ui;
    ViewConfig view;
    InputConfig input = defaultInputConfig();
};

// Upgrades a config document in place to kConfigSchemaVersion. Documents without "schemaVersion" are
// treated as version 0. Documents from a newer version are left untouched.
void migrateConfig(nlohmann::json& doc);

// Reads the known fields of `doc` over the defaults. Fields with the wrong type are ignored and reported.
Config configFromJson(const nlohmann::json& doc, std::vector<std::string>* warnings = nullptr);

// Writes `config` into `doc`, keeping every key the program does not know about.
void configToJson(const Config& config, nlohmann::json& doc);

// A config file on disk. The parsed JSON document is retained so unknown keys survive a save.
class ConfigFile {
public:
    // Loads `path`. If the file is missing, defaults are written to it. If it is unreadable or not valid
    // JSON, it is renamed to "<name>.bad" and defaults are used.
    static ConfigFile loadOrCreate(const std::filesystem::path& path);

    Config config;
    std::vector<std::string> warnings;  // problems found while loading

    // Returns false (and appends to warnings) if the file cannot be written.
    bool save();

    const std::filesystem::path& path() const { return path_; }
    const nlohmann::json& document() const { return doc_; }

private:
    std::filesystem::path path_;
    nlohmann::json doc_ = nlohmann::json::object();
};

}  // namespace gen::common
