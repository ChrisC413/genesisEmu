// gen_cli: headless runner for automation and CI.

#include <algorithm>
#include <charconv>
#include <cstdio>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "common/logging.hpp"
#include "common/rom_info.hpp"
#include "core/cart/rom.hpp"
#include "core/version.hpp"

namespace {

enum ExitCode : int {
    kExitOk = 0,
    kExitUsage = 1,
    kExitLoadError = 2,
    kExitCheckFailure = 3,
};

constexpr std::string_view kUsage = R"(Usage: gen_cli <command> [options]

Commands:
  info --rom <file> [--json]
      Load a ROM and print its header, SHA-1, CRC32 and identified game.
      --json   print machine-readable JSON instead of text

  run [--rom <file>] [--frames <N>]
      Run the emulator headless for N frames (default 1). The emulation core
      arrives in M02; until then this only loads the ROM (if given) and exits.

  help [command]
      Show this help.

Planned: trace, coverage, export.

Global options:
  -h, --help      show help
  --version       print the version

Exit codes: 0 ok, 1 usage error, 2 ROM load error, 3 check failure.
)";

struct Args {
    std::vector<std::string_view> positional;
    std::optional<std::string_view> rom;
    std::optional<std::string_view> frames;
    bool json = false;
    bool help = false;
    bool version = false;
    std::string error;
};

Args parseArgs(int argc, char** argv) {
    Args a;
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];
        const auto value = [&](std::optional<std::string_view>& out) {
            if (i + 1 >= argc) {
                a.error = "missing value for " + std::string(arg);
                return;
            }
            out = argv[++i];
        };
        if (arg == "-h" || arg == "--help") {
            a.help = true;
        } else if (arg == "--version") {
            a.version = true;
        } else if (arg == "--rom") {
            value(a.rom);
        } else if (arg == "--frames") {
            value(a.frames);
        } else if (arg == "--json") {
            a.json = true;
        } else if (arg.starts_with("-")) {
            a.error = "unknown option " + std::string(arg);
        } else {
            a.positional.push_back(arg);
        }
        if (!a.error.empty()) {
            break;
        }
    }
    return a;
}

int usageError(const std::string& message) {
    std::fprintf(stderr, "gen_cli: %s\n\nRun 'gen_cli --help' for usage.\n", message.c_str());
    return kExitUsage;
}

std::optional<gen::cart::Rom> loadOrReport(std::string_view path) {
    auto result = gen::cart::loadRomFile(std::filesystem::path(std::string(path)));
    if (!result) {
        std::fprintf(stderr, "gen_cli: %s\n", result.error.c_str());
        return std::nullopt;
    }
    for (const auto& warning : result.rom->warnings) {
        gen::log::get(gen::log::Subsystem::Cli).warn("{}", warning);
    }
    return std::move(result.rom);
}

int cmdInfo(const Args& a) {
    if (!a.rom) {
        return usageError("info requires --rom <file>");
    }
    const auto rom = loadOrReport(*a.rom);
    if (!rom) {
        return kExitLoadError;
    }
    if (a.json) {
        std::printf("%s\n", gen::common::romToJson(*rom).dump(2).c_str());
        return kExitOk;
    }
    std::size_t width = 0;
    const auto fields = gen::common::describeRom(*rom);
    for (const auto& f : fields) {
        width = std::max(width, f.label.size());
    }
    for (const auto& f : fields) {
        std::printf("%-*s  %s\n", static_cast<int>(width), f.label.c_str(), f.value.c_str());
    }
    return kExitOk;
}

int cmdRun(const Args& a) {
    unsigned long long frames = 1;
    if (a.frames) {
        const auto* end = a.frames->data() + a.frames->size();
        const auto [ptr, ec] = std::from_chars(a.frames->data(), end, frames);
        if (ec != std::errc() || ptr != end) {
            return usageError("--frames expects a non-negative integer");
        }
    }
    if (a.rom && !loadOrReport(*a.rom)) {
        return kExitLoadError;
    }
    std::printf("run: emulation core not implemented yet (M02); %llu frame(s) requested, 0 executed\n",
                frames);
    return kExitOk;
}

}  // namespace

int main(int argc, char** argv) {
    gen::common::LogConfig logConfig;
    logConfig.level = "warn";
    gen::log::init(logConfig, {.console = true, .logDir = std::nullopt});

    const Args a = parseArgs(argc, argv);
    if (!a.error.empty()) {
        return usageError(a.error);
    }
    if (a.version) {
        std::printf("gen_cli %.*s\n", static_cast<int>(gen::version().size()), gen::version().data());
        return kExitOk;
    }
    if (a.help || a.positional.empty()) {
        std::fputs(kUsage.data(), a.positional.empty() && !a.help ? stderr : stdout);
        return a.help ? kExitOk : kExitUsage;
    }

    const std::string_view command = a.positional.front();
    if (command == "help") {
        std::fputs(kUsage.data(), stdout);
        return kExitOk;
    }
    if (a.positional.size() > 1) {
        return usageError("unexpected argument '" + std::string(a.positional[1]) + "'");
    }
    if (command == "info") {
        return cmdInfo(a);
    }
    if (command == "run") {
        return cmdRun(a);
    }
    return usageError("unknown command '" + std::string(command) + "'");
}
