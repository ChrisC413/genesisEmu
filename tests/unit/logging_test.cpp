#include "common/logging.hpp"

#include <fstream>
#include <iterator>

#include <catch2/catch_test_macros.hpp>

#include "test_support.hpp"

using gen::log::Subsystem;

TEST_CASE("Log levels parse", "[logging]") {
    CHECK(gen::log::parseLevel("trace") == spdlog::level::trace);
    CHECK(gen::log::parseLevel("warning") == spdlog::level::warn);
    CHECK(gen::log::parseLevel("off") == spdlog::level::off);
    CHECK_FALSE(gen::log::parseLevel("loud"));
}

TEST_CASE("Per-subsystem log levels come from the config", "[logging]") {
    gen::common::LogConfig config;
    config.level = "warn";
    config.subsystems = {{"vdp", "trace"}, {"cpu", "error"}};

    const auto warnings = gen::log::init(config, {.console = false, .logDir = std::nullopt});
    CHECK(warnings.empty());
    CHECK(gen::log::get(Subsystem::Vdp).level() == spdlog::level::trace);
    CHECK(gen::log::get(Subsystem::Cpu).level() == spdlog::level::err);
    CHECK(gen::log::get(Subsystem::Audio).level() == spdlog::level::warn);
    gen::log::shutdown();
}

TEST_CASE("Invalid level names are reported", "[logging]") {
    gen::common::LogConfig config;
    config.level = "verbose";
    config.subsystems = {{"vdp", "loud"}, {"gpu", "info"}};

    const auto warnings = gen::log::init(config, {.console = false, .logDir = std::nullopt});
    CHECK(warnings.size() == 3);
    CHECK(gen::log::get(Subsystem::Vdp).level() == spdlog::level::info);
    gen::log::shutdown();
}

TEST_CASE("Messages reach the rotating log file", "[logging]") {
    gen::test::TempDir dir;
    {
        gen::common::LogConfig config;
        config.subsystems = {{"bus", "debug"}};
        gen::log::init(config, {.console = false, .logDir = dir.path()});
        gen::log::get(Subsystem::Bus).debug("bus message {}", 42);
        gen::log::get(Subsystem::Vdp).debug("filtered out");
        gen::log::shutdown();
    }
    std::ifstream in(dir.path() / "genesisEmu.log");
    const std::string text{std::istreambuf_iterator<char>(in), {}};
    CHECK(text.find("[bus] [debug] bus message 42") != std::string::npos);
    CHECK(text.find("filtered out") == std::string::npos);
}
