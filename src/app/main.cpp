#include <charconv>
#include <cstdio>
#include <exception>
#include <string_view>

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "app/app.hpp"

namespace {

constexpr const char* kUsage =
    "Usage: genesisEmu [options] [rom]\n"
    "\n"
    "Options:\n"
    "  --smoke-test[=N]          render N frames (default 60), run self-checks and exit\n"
    "  --expect-restored-layout  smoke check: fail unless a saved dock layout was loaded\n"
    "  --screenshot=<file>       smoke test: save the last frame as PNG\n"
    "  --lock-on=<file>          cartridge locked onto the ROM (Sonic & Knuckles + Sonic 3)\n"
    "  --state=<file>            load a save state (from the same ROM) right after the ROM\n"
    "  --fast-forward            start in fast-forward (unthrottled)\n"
    "  --debugger                open the debugger panels (Debug > Debugger Layout)\n"
    "  --pause-at=<frame>        pause when the emulation reaches this frame\n"
    "  --break=<hex address>     set a 68k execution breakpoint (repeatable)\n"
    "  --sprite-boxes            outline the sprites on the game view\n"
    "  --focus=<window>          bring a panel to the front, e.g. --focus=Tiles (repeatable)\n"
    "  -h, --help                show this help\n"
    "\n"
    "Set GENESISEMU_HOME to use a different user data directory.\n";

void report(const char* message) {
    // A GUI-subsystem exe on Windows has no console, so also log through SDL.
    std::fputs(message, stderr);
    SDL_Log("%s", message);
}

}  // namespace

int main(int argc, char** argv) {
    gen::app::AppOptions options;
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            report(kUsage);
            return gen::app::kAppOk;
        }
        if (arg == "--smoke-test") {
            options.smokeFrames = 60;
        } else if (arg.starts_with("--smoke-test=")) {
            const std::string_view value = arg.substr(13);
            int frames = 0;
            const auto [ptr, ec] = std::from_chars(value.data(), value.data() + value.size(), frames);
            if (ec != std::errc() || ptr != value.data() + value.size() || frames <= 0) {
                report("genesisEmu: --smoke-test expects a positive frame count\n");
                return gen::app::kAppUsage;
            }
            options.smokeFrames = frames;
        } else if (arg.starts_with("--screenshot=")) {
            options.screenshot = gen::common::fromUtf8(arg.substr(13));
        } else if (arg.starts_with("--lock-on=")) {
            options.lockOn = gen::common::fromUtf8(arg.substr(10));
        } else if (arg.starts_with("--state=")) {
            options.state = gen::common::fromUtf8(arg.substr(8));
        } else if (arg == "--sprite-boxes") {
            options.spriteBoxes = true;
        } else if (arg == "--debugger") {
            options.debugger = true;
        } else if (arg.starts_with("--pause-at=")) {
            const std::string_view value = arg.substr(11);
            std::uint64_t frame = 0;
            const auto [ptr, ec] = std::from_chars(value.data(), value.data() + value.size(), frame);
            if (ec != std::errc() || ptr != value.data() + value.size()) {
                report("genesisEmu: --pause-at expects a frame number\n");
                return gen::app::kAppUsage;
            }
            options.pauseAt = frame;
        } else if (arg.starts_with("--break=")) {
            std::string_view value = arg.substr(8);
            if (value.starts_with("$")) {
                value.remove_prefix(1);
            } else if (value.starts_with("0x")) {
                value.remove_prefix(2);
            }
            std::uint32_t addr = 0;
            const auto [ptr, ec] = std::from_chars(value.data(), value.data() + value.size(), addr, 16);
            if (ec != std::errc() || ptr != value.data() + value.size()) {
                report("genesisEmu: --break expects a hex address\n");
                return gen::app::kAppUsage;
            }
            options.breakAt.push_back(addr);
        } else if (arg.starts_with("--focus=")) {
            options.focusWindows.emplace_back(arg.substr(8));
        } else if (arg == "--fast-forward") {
            options.fastForward = true;
        } else if (arg == "--expect-restored-layout") {
            options.expectRestoredLayout = true;
        } else if (arg.starts_with("-")) {
            report(kUsage);
            return gen::app::kAppUsage;
        } else {
            // argv is UTF-8 on macOS; on Windows SDL_main converts the wide command line to UTF-8.
            options.rom = gen::common::fromUtf8(arg);
        }
    }

    try {
        gen::app::App app(gen::common::AppPaths::fromEnvironment());
        return app.run(options);
    } catch (const std::exception& e) {
        report(e.what());
        return gen::app::kAppUsage;
    }
}
