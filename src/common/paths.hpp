#pragma once

#include <filesystem>
#include <functional>
#include <optional>
#include <string_view>

namespace gen::common {

enum class Platform { Windows, MacOS, Linux };

Platform currentPlatform();

inline constexpr std::string_view kAppDirName = "genesisEmu";

// Environment variable that overrides the user data root (tests, portable installs).
inline constexpr std::string_view kHomeOverrideEnv = "GENESISEMU_HOME";

using EnvLookup = std::function<std::optional<std::filesystem::path>(std::string_view name)>;

// Reads an environment variable as a path (wide on Windows so non-ASCII user names work).
std::optional<std::filesystem::path> getEnvPath(std::string_view name);

// Resolves the per-user root directory for `platform`:
//   GENESISEMU_HOME if set, else
//   Windows: %APPDATA%\genesisEmu
//   macOS:   $HOME/Library/Application Support/genesisEmu
//   Linux:   $XDG_CONFIG_HOME/genesisEmu or $HOME/.config/genesisEmu
// Returns nullopt if the required variables are missing.
std::optional<std::filesystem::path> userDataRootFor(Platform platform, const EnvLookup& env);

// Directory layout under the user data root.
class AppPaths {
public:
    explicit AppPaths(std::filesystem::path root) : root_(std::move(root)) {}

    // Uses the real environment of the current platform; throws std::runtime_error if unresolvable.
    static AppPaths fromEnvironment();

    const std::filesystem::path& root() const { return root_; }
    std::filesystem::path configFile() const { return root_ / "config.json"; }
    std::filesystem::path logDir() const { return root_ / "logs"; }
    // Per-ROM data (SRAM, save states, project) keyed by lowercase SHA-1.
    std::filesystem::path romDataDir(std::string_view sha1Hex) const { return root_ / "roms" / sha1Hex; }

private:
    std::filesystem::path root_;
};

}  // namespace gen::common
