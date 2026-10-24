#pragma once

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

struct Config {
    LogConfig log;
    UiConfig ui;
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
