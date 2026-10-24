#include "common/paths.hpp"

#include <map>
#include <string>

#include <catch2/catch_test_macros.hpp>

using gen::common::Platform;
using gen::common::userDataRootFor;
namespace fs = std::filesystem;

namespace {

gen::common::EnvLookup fakeEnv(std::map<std::string, fs::path> vars) {
    return [vars = std::move(vars)](std::string_view name) -> std::optional<fs::path> {
        const auto it = vars.find(std::string(name));
        return it == vars.end() ? std::nullopt : std::optional<fs::path>(it->second);
    };
}

}  // namespace

TEST_CASE("Windows user data root is %APPDATA%\\genesisEmu", "[paths]") {
    const auto root =
        userDataRootFor(Platform::Windows, fakeEnv({{"APPDATA", "C:/Users/me/AppData/Roaming"}}));
    REQUIRE(root);
    CHECK(root->generic_string() == "C:/Users/me/AppData/Roaming/genesisEmu");
}

TEST_CASE("macOS user data root is ~/Library/Application Support/genesisEmu", "[paths]") {
    const auto root = userDataRootFor(Platform::MacOS, fakeEnv({{"HOME", "/Users/me"}}));
    REQUIRE(root);
    CHECK(root->generic_string() == "/Users/me/Library/Application Support/genesisEmu");
}

TEST_CASE("Linux user data root follows XDG", "[paths]") {
    CHECK(userDataRootFor(Platform::Linux, fakeEnv({{"HOME", "/home/me"}}))->generic_string() ==
          "/home/me/.config/genesisEmu");
    CHECK(userDataRootFor(Platform::Linux, fakeEnv({{"HOME", "/home/me"}, {"XDG_CONFIG_HOME", "/cfg"}}))
              ->generic_string() == "/cfg/genesisEmu");
}

TEST_CASE("GENESISEMU_HOME overrides the platform default", "[paths]") {
    const auto env = fakeEnv({{"GENESISEMU_HOME", "/portable"}, {"APPDATA", "C:/x"}, {"HOME", "/h"}});
    CHECK(userDataRootFor(Platform::Windows, env)->generic_string() == "/portable");
    CHECK(userDataRootFor(Platform::MacOS, env)->generic_string() == "/portable");
}

TEST_CASE("Missing environment yields no root", "[paths]") {
    CHECK_FALSE(userDataRootFor(Platform::Windows, fakeEnv({})));
    CHECK_FALSE(userDataRootFor(Platform::MacOS, fakeEnv({})));
}

TEST_CASE("Per-ROM data folders are keyed by SHA-1", "[paths]") {
    const gen::common::AppPaths paths("/root");
    CHECK(paths.configFile().generic_string() == "/root/config.json");
    CHECK(paths.logDir().generic_string() == "/root/logs");
    CHECK(paths.romDataDir("69e102855d4389c3fd1a8f3dc7d193f8eee5fe5b").generic_string() ==
          "/root/roms/69e102855d4389c3fd1a8f3dc7d193f8eee5fe5b");
}

TEST_CASE("Paths round-trip through UTF-8", "[paths]") {
    const std::string utf8 = "roms/S\xC3\xB6nic \xE3\x82\xBD\xE3\x83\x8B\xE3\x83\x83\xE3\x82\xAF.bin";
    CHECK(gen::common::toUtf8(gen::common::fromUtf8(utf8)) == utf8);
    CHECK(gen::common::fromUtf8(utf8).filename().extension() == ".bin");
}

TEST_CASE("The real environment resolves on this platform", "[paths]") {
    const auto paths = gen::common::AppPaths::fromEnvironment();
    CHECK(paths.root().is_absolute());
    if (!gen::common::getEnvPath(gen::common::kHomeOverrideEnv)) {
        CHECK(paths.root().filename() == "genesisEmu");
#if defined(_WIN32)
        CHECK(paths.root().parent_path() == *gen::common::getEnvPath("APPDATA"));
#elif defined(__APPLE__)
        CHECK(paths.root().parent_path().filename() == "Application Support");
#endif
    }
}
