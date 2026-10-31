#include "common/logging.hpp"

#include <algorithm>
#include <mutex>

#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace gen::log {

namespace {

constexpr std::size_t kCount = static_cast<std::size_t>(Subsystem::Count);

struct State {
    std::mutex mutex;
    std::array<std::shared_ptr<spdlog::logger>, kCount> loggers;
};

State& state() {
    static State s;
    return s;
}

std::shared_ptr<spdlog::logger> makeLogger(std::string_view loggerName, std::vector<spdlog::sink_ptr> sinks) {
    auto logger = std::make_shared<spdlog::logger>(std::string(loggerName), sinks.begin(), sinks.end());
    logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%l] %v");
    logger->flush_on(spdlog::level::warn);
    return logger;
}

std::vector<std::string> applyLevelsLocked(State& s, const common::LogConfig& config) {
    std::vector<std::string> warnings;
    auto defaultLevel = parseLevel(config.level);
    if (!defaultLevel) {
        warnings.push_back("unknown log level '" + config.level + "'; using 'info'");
        defaultLevel = spdlog::level::info;
    }
    for (std::size_t i = 0; i < kCount; ++i) {
        if (s.loggers[i]) {
            s.loggers[i]->set_level(*defaultLevel);
        }
    }
    for (const auto& [subsystem, levelText] : config.subsystems) {
        std::size_t index = kCount;
        for (std::size_t i = 0; i < kCount; ++i) {
            if (kSubsystemNames[i] == subsystem) {
                index = i;
            }
        }
        const auto level = parseLevel(levelText);
        if (index == kCount) {
            warnings.push_back("unknown log subsystem '" + subsystem + "'");
        } else if (!level) {
            warnings.push_back("unknown log level '" + levelText + "' for subsystem '" + subsystem + "'");
        } else if (s.loggers[index]) {
            s.loggers[index]->set_level(*level);
        }
    }
    return warnings;
}

}  // namespace

std::optional<spdlog::level::level_enum> parseLevel(std::string_view text) {
    static constexpr std::array<std::pair<std::string_view, spdlog::level::level_enum>, 8> kLevels{{
        {"trace", spdlog::level::trace},
        {"debug", spdlog::level::debug},
        {"info", spdlog::level::info},
        {"warn", spdlog::level::warn},
        {"warning", spdlog::level::warn},
        {"error", spdlog::level::err},
        {"critical", spdlog::level::critical},
        {"off", spdlog::level::off},
    }};
    for (const auto& [levelName, level] : kLevels) {
        if (levelName == text) {
            return level;
        }
    }
    return std::nullopt;
}

std::vector<std::string> init(const common::LogConfig& config, const Options& options) {
    State& s = state();
    std::lock_guard lock(s.mutex);
    std::vector<std::string> warnings;

    std::vector<spdlog::sink_ptr> sinks;
    if (options.console) {
        sinks.push_back(std::make_shared<spdlog::sinks::stderr_color_sink_mt>());
    }
    if (options.logDir && config.toFile) {
        try {
            std::filesystem::create_directories(*options.logDir);
            const std::size_t maxBytes =
                std::size_t{std::max<std::uint32_t>(config.maxFileSizeKb, 16)} * 1024;
            const std::size_t maxFiles = std::max<std::uint32_t>(config.maxFiles, 1);
            sinks.push_back(std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                (*options.logDir / "genesisEmu.log").string(), maxBytes, maxFiles));
        } catch (const std::exception& e) {
            warnings.push_back(std::string("cannot open log file: ") + e.what());
        }
    }

    for (std::size_t i = 0; i < kCount; ++i) {
        s.loggers[i] = makeLogger(kSubsystemNames[i], sinks);
    }
    auto levelWarnings = applyLevelsLocked(s, config);
    warnings.insert(warnings.end(), levelWarnings.begin(), levelWarnings.end());
    return warnings;
}

std::vector<std::string> applyLevels(const common::LogConfig& config) {
    State& s = state();
    std::lock_guard lock(s.mutex);
    return applyLevelsLocked(s, config);
}

spdlog::logger& get(Subsystem sub) {
    State& s = state();
    std::lock_guard lock(s.mutex);
    auto& logger = s.loggers[static_cast<std::size_t>(sub)];
    if (!logger) {
        logger = makeLogger(name(sub), {std::make_shared<spdlog::sinks::stderr_color_sink_mt>()});
    }
    return *logger;
}

void shutdown() {
    State& s = state();
    std::lock_guard lock(s.mutex);
    for (auto& logger : s.loggers) {
        if (logger) {
            logger->flush();
        }
        logger.reset();
    }
}

}  // namespace gen::log
