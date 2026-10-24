#include "common/paths.hpp"

#include <cstdlib>
#include <stdexcept>
#include <string>

namespace gen::common {

Platform currentPlatform() {
#if defined(_WIN32)
    return Platform::Windows;
#elif defined(__APPLE__)
    return Platform::MacOS;
#else
    return Platform::Linux;
#endif
}

std::string toUtf8(const std::filesystem::path& path) {
    const std::u8string u8 = path.u8string();
    return {u8.begin(), u8.end()};
}

std::filesystem::path fromUtf8(std::string_view utf8) {
    return std::filesystem::path(std::u8string(utf8.begin(), utf8.end()));
}

std::optional<std::filesystem::path> getEnvPath(std::string_view name) {
#if defined(_WIN32)
    const std::wstring wideName(name.begin(), name.end());
    wchar_t* value = nullptr;
    std::size_t len = 0;
    if (_wdupenv_s(&value, &len, wideName.c_str()) != 0 || value == nullptr) {
        return std::nullopt;
    }
    std::filesystem::path result(value);
    std::free(value);
#else
    const char* value = std::getenv(std::string(name).c_str());
    if (value == nullptr) {
        return std::nullopt;
    }
    std::filesystem::path result(value);
#endif
    if (result.empty()) {
        return std::nullopt;
    }
    return result;
}

std::optional<std::filesystem::path> userDataRootFor(Platform platform, const EnvLookup& env) {
    if (auto overridden = env(kHomeOverrideEnv)) {
        return *overridden;
    }
    switch (platform) {
        case Platform::Windows:
            if (auto appData = env("APPDATA")) {
                return *appData / kAppDirName;
            }
            return std::nullopt;
        case Platform::MacOS:
            if (auto home = env("HOME")) {
                return *home / "Library" / "Application Support" / kAppDirName;
            }
            return std::nullopt;
        case Platform::Linux:
            if (auto xdg = env("XDG_CONFIG_HOME")) {
                return *xdg / kAppDirName;
            }
            if (auto home = env("HOME")) {
                return *home / ".config" / kAppDirName;
            }
            return std::nullopt;
    }
    return std::nullopt;
}

AppPaths AppPaths::fromEnvironment() {
    auto root = userDataRootFor(currentPlatform(), getEnvPath);
    if (!root) {
        throw std::runtime_error("Cannot determine the user data directory (APPDATA/HOME not set)");
    }
    return AppPaths(*root);
}

}  // namespace gen::common
