#pragma once

#include <array>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <spdlog/spdlog.h>

#include "common/config.hpp"

namespace gen::log {

// Subsystems with independently configurable levels (spec 07, Logging).
enum class Subsystem { App, Cpu, Vdp, Bus, Audio, Analysis, Llm, Config, Cli, Count };

inline constexpr std::array<std::string_view, static_cast<std::size_t>(Subsystem::Count)> kSubsystemNames{
    "app", "cpu", "vdp", "bus", "audio", "analysis", "llm", "config", "cli"};

constexpr std::string_view name(Subsystem s) {
    return kSubsystemNames[static_cast<std::size_t>(s)];
}

std::optional<spdlog::level::level_enum> parseLevel(std::string_view text);

struct Options {
    bool console = true;
    std::optional<std::filesystem::path> logDir;  // rotating file "genesisEmu.log" when set
};

// (Re)creates one logger per subsystem sharing a console and/or rotating file sink, and applies the
// levels from `config`. Returns warnings about invalid level names.
std::vector<std::string> init(const common::LogConfig& config, const Options& options);

// Re-applies levels without recreating sinks. Returns warnings about invalid level names.
std::vector<std::string> applyLevels(const common::LogConfig& config);

// Logger for a subsystem. Safe to call before init(): it then logs to stderr at "info".
spdlog::logger& get(Subsystem s);

void shutdown();

}  // namespace gen::log
