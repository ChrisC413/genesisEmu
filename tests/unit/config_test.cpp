#include "common/config.hpp"

#include <fstream>
#include <sstream>

#include <catch2/catch_test_macros.hpp>

#include "test_support.hpp"

using gen::common::Config;
using gen::common::ConfigFile;
using nlohmann::json;

namespace {

json readJson(const std::filesystem::path& p) {
    std::ifstream in(p);
    return json::parse(in);
}

void writeText(const std::filesystem::path& p, const std::string& text) {
    std::ofstream out(p, std::ios::binary);
    out << text;
}

}  // namespace

TEST_CASE("Missing config writes defaults on first run", "[config]") {
    gen::test::TempDir dir;
    const auto path = dir.path() / "sub" / "config.json";

    const ConfigFile file = ConfigFile::loadOrCreate(path);

    REQUIRE(std::filesystem::exists(path));
    CHECK(file.warnings.empty());
    const json doc = readJson(path);
    CHECK(doc["schemaVersion"] == gen::common::kConfigSchemaVersion);
    CHECK(doc["log"]["level"] == "info");
    CHECK(doc["ui"]["windowWidth"] == 1280);
    CHECK(doc["ui"]["recentRoms"].is_array());
}

TEST_CASE("Config round-trips through disk", "[config]") {
    gen::test::TempDir dir;
    const auto path = dir.path() / "config.json";

    ConfigFile file = ConfigFile::loadOrCreate(path);
    file.config.log.level = "debug";
    file.config.log.subsystems["vdp"] = "trace";
    file.config.ui.windowWidth = 1600;
    file.config.ui.recentRoms = {"C:/roms/s1.bin", "/Users/me/s2.md"};
    file.config.ui.dockLayout = "[Window][ROM Info]\nPos=10,20\n";
    REQUIRE(file.save());

    const ConfigFile reloaded = ConfigFile::loadOrCreate(path);
    CHECK(reloaded.warnings.empty());
    CHECK(reloaded.config.log.level == "debug");
    CHECK(reloaded.config.log.subsystems.at("vdp") == "trace");
    CHECK(reloaded.config.ui.windowWidth == 1600);
    CHECK(reloaded.config.ui.recentRoms == file.config.ui.recentRoms);
    CHECK(reloaded.config.ui.dockLayout == file.config.ui.dockLayout);
}

TEST_CASE("Unknown keys are preserved on save", "[config]") {
    gen::test::TempDir dir;
    const auto path = dir.path() / "config.json";
    writeText(path, R"({
        "schemaVersion": 1,
        "futureFeature": {"enabled": true, "list": [1, 2, 3]},
        "log": {"level": "warn", "colour": "auto"},
        "ui": {"windowWidth": 900, "theme": "dark"}
    })");

    ConfigFile file = ConfigFile::loadOrCreate(path);
    CHECK(file.config.log.level == "warn");
    CHECK(file.config.ui.windowWidth == 900);
    file.config.ui.windowWidth = 1000;
    REQUIRE(file.save());

    const json doc = readJson(path);
    CHECK(doc["futureFeature"]["enabled"] == true);
    CHECK(doc["futureFeature"]["list"] == json::array({1, 2, 3}));
    CHECK(doc["log"]["colour"] == "auto");
    CHECK(doc["ui"]["theme"] == "dark");
    CHECK(doc["ui"]["windowWidth"] == 1000);
}

TEST_CASE("Unversioned config is migrated to the current schema", "[config]") {
    gen::test::TempDir dir;
    const auto path = dir.path() / "config.json";
    writeText(path, R"({"log": {"level": "error"}, "custom": 5})");

    ConfigFile file = ConfigFile::loadOrCreate(path);
    CHECK(file.config.log.level == "error");
    REQUIRE(file.save());

    const json doc = readJson(path);
    CHECK(doc["schemaVersion"] == gen::common::kConfigSchemaVersion);
    CHECK(doc["custom"] == 5);
}

TEST_CASE("Config from a newer schema keeps its version", "[config]") {
    json doc = {{"schemaVersion", 99}, {"log", {{"level", "debug"}}}};
    gen::common::migrateConfig(doc);
    CHECK(doc["schemaVersion"] == 99);

    Config c = gen::common::configFromJson(doc);
    gen::common::configToJson(c, doc);
    CHECK(doc["schemaVersion"] == 99);
}

TEST_CASE("Wrong-typed fields fall back to defaults with a warning", "[config]") {
    const json doc = {{"log", {{"level", 3}}}, {"ui", {{"windowWidth", "wide"}, {"recentRoms", "x"}}}};
    std::vector<std::string> warnings;
    const Config c = gen::common::configFromJson(doc, &warnings);
    CHECK(c.log.level == "info");
    CHECK(c.ui.windowWidth == 1280);
    CHECK(c.ui.recentRoms.empty());
    CHECK(warnings.size() == 3);
}

TEST_CASE("Corrupt config is moved aside and replaced by defaults", "[config]") {
    gen::test::TempDir dir;
    const auto path = dir.path() / "config.json";
    writeText(path, "{ not json");

    const ConfigFile file = ConfigFile::loadOrCreate(path);
    CHECK(file.warnings.size() == 1);
    CHECK(std::filesystem::exists(dir.path() / "config.json.bad"));
    CHECK(readJson(path)["log"]["level"] == "info");
}

TEST_CASE("View settings and input bindings round-trip (M04-02)", "[config][input]") {
    gen::test::TempDir dir;
    const auto path = dir.path() / "config.json";

    ConfigFile file = ConfigFile::loadOrCreate(path);
    CHECK(file.config.input.keys[0].at("START") == "Return");  // spec 07 default
    CHECK(file.config.input.hotkeys.at("pause") == "F5");
    file.config.view.scale = "integer";
    file.config.view.linearFilter = true;
    file.config.input.padType[1] = "6";
    file.config.input.keys[0]["C"] = "Space";
    file.config.input.keys[1]["START"] = "Keypad Enter";
    file.config.input.gamepad["A"] = "y";
    REQUIRE(file.save());

    const ConfigFile reloaded = ConfigFile::loadOrCreate(path);
    CHECK(reloaded.warnings.empty());
    CHECK(reloaded.config.view.scale == "integer");
    CHECK(reloaded.config.view.linearFilter);
    CHECK(reloaded.config.input.padType[1] == "6");
    CHECK(reloaded.config.input.keys[0].at("C") == "Space");
    CHECK(reloaded.config.input.keys[1].at("START") == "Keypad Enter");
    CHECK(reloaded.config.input.gamepad.at("A") == "y");
    CHECK(reloaded.config.input.hotkeys.at("step-frame") == "F6");
}
