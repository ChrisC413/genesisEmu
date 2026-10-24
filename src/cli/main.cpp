// gen_cli: headless runner for automation and CI.

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

#include "analysis/disasm68k/disasm68k.hpp"
#include "common/logging.hpp"
#include "common/rom_info.hpp"
#include "core/cart/rom.hpp"
#include "core/sched/machine.hpp"
#include "core/state/state_hash.hpp"
#include "core/version.hpp"
#include "instrument/trace_writer.hpp"

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

  run --rom <file> [--lock-on <file>] [--frames <N>] [--region <r>] [--json]
      Run the emulator headless for N frames (default 1) and print frames,
      instructions, time per frame, the state hash, exceptions and bus issues.
      Exits 3 when the 68k took a fault exception (address error, illegal...).
      --lock-on <file>  cartridge locked onto --rom (S&K + Sonic 3 = S3K)
      --region <r>      japan | americas | europe (default: from the header)
      --frame-log       print V-int count and PCs after every frame

  trace --rom <file> [--lock-on <file>] [--frames <N>] [--start-frame <S>]
        [--pc <lo>-<hi>] [--first] [--max-records <N>] [--out <file>]
      Run N frames (default 1) and write a text instruction trace (format in
      docs/specs/08-debugger.md) of frames S..N-1 to --out (default stdout).
      --pc           only log instructions whose PC is in [lo, hi] (hex)
      --first        log each PC only the first time it executes
      --max-records  stop logging after N records

  help [command]
      Show this help.

Planned: coverage, export.

Global options:
  -h, --help      show help
  --version       print the version

Exit codes: 0 ok, 1 usage error, 2 ROM load error, 3 check failure.
)";

struct Args {
    std::vector<std::string_view> positional;
    std::optional<std::string_view> rom;
    std::optional<std::string_view> frames;
    std::optional<std::string_view> lockOn;
    std::optional<std::string_view> region;
    std::optional<std::string_view> startFrame;
    std::optional<std::string_view> pcRange;
    std::optional<std::string_view> out;
    bool json = false;
    bool frameLog = false;
    bool firstOnly = false;
    std::optional<std::string_view> maxRecords;
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
        } else if (arg == "--lock-on") {
            value(a.lockOn);
        } else if (arg == "--region") {
            value(a.region);
        } else if (arg == "--start-frame") {
            value(a.startFrame);
        } else if (arg == "--pc") {
            value(a.pcRange);
        } else if (arg == "--out") {
            value(a.out);
        } else if (arg == "--first") {
            a.firstOnly = true;
        } else if (arg == "--max-records") {
            value(a.maxRecords);
        } else if (arg == "--frame-log") {
            a.frameLog = true;
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

bool parseU64(std::optional<std::string_view> text, std::uint64_t& out, int base = 10) {
    if (!text) {
        return true;
    }
    const auto* end = text->data() + text->size();
    const auto [ptr, ec] = std::from_chars(text->data(), end, out, base);
    return ec == std::errc() && ptr == end && !text->empty();
}

struct LoadedMachine {
    std::unique_ptr<gen::Machine> machine;
    std::string title;
};

// Loads --rom (plus --lock-on) into a Machine; prints the error and returns an exit code on failure.
int loadMachine(const Args& a, LoadedMachine& out) {
    if (!a.rom) {
        return usageError("this command requires --rom <file>");
    }
    auto rom = loadOrReport(*a.rom);
    if (!rom) {
        return kExitLoadError;
    }
    std::vector<std::uint8_t> image = rom->data;
    std::uint8_t regions = rom->header.regions;
    out.title = rom->known != nullptr ? std::string(rom->known->title) : rom->header.overseasTitle;
    if (a.lockOn) {
        const auto locked = loadOrReport(*a.lockOn);
        if (!locked) {
            return kExitLoadError;
        }
        if (const auto* combined = gen::cart::identifyLockOn(rom->data, locked->data)) {
            out.title = std::string(combined->title);
        }
        image = gen::cart::lockOnImage(rom->data, locked->data);
        regions = locked->header.regions;
    }
    gen::MachineConfig config;
    if (a.region) {
        if (*a.region == "japan") {
            config.region = gen::bus::Region::Japan;
        } else if (*a.region == "americas") {
            config.region = gen::bus::Region::Americas;
        } else if (*a.region == "europe") {
            config.region = gen::bus::Region::Europe;
        } else {
            return usageError("--region expects japan, americas or europe");
        }
    }
    out.machine = std::make_unique<gen::Machine>(std::move(image), regions, config);
    return kExitOk;
}

int cmdRun(const Args& a) {
    std::uint64_t frames = 1;
    if (!parseU64(a.frames, frames)) {
        return usageError("--frames expects a non-negative integer");
    }
    LoadedMachine lm;
    if (const int rc = loadMachine(a, lm); rc != kExitOk) {
        return rc;
    }
    gen::Machine& m = *lm.machine;
    const auto t0 = std::chrono::steady_clock::now();
    gen::RunResult r;
    if (a.frameLog) {
        for (std::uint64_t f = 0; f < frames; ++f) {
            r.instructions += m.run(gen::RunLimit::frames(1)).instructions;
            std::printf("frame %llu: V-ints %u, last V-int from PC $%06X, PC $%06X\n",
                        static_cast<unsigned long long>(f), m.stats().vintsLastFrame, m.stats().lastVintPc,
                        m.cpu().state().pc);
        }
    } else {
        r = m.run(gen::RunLimit::frames(frames));
    }
    const double ms =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    const auto& st = m.stats();
    const auto& cpu = m.cpu().state();
    const std::string hash = gen::state::hashToHex(m.stateHash());
    const double msPerFrame = frames != 0 ? ms / static_cast<double>(frames) : 0.0;
    if (a.json) {
        nlohmann::json j;
        j["title"] = lm.title;
        j["frames"] = m.frame();
        j["instructions"] = r.instructions;
        j["ms"] = ms;
        j["msPerFrame"] = msPerFrame;
        j["stateHash"] = hash;
        j["pc"] = cpu.pc;
        j["vints"] = st.vints;
        j["lastVintPc"] = st.lastVintPc;
        j["faults"] = st.faults;
        j["firstFaultVector"] = st.firstFaultVector;
        j["firstFaultPc"] = st.firstFaultPc;
        j["halted"] = cpu.halted;
        nlohmann::json issues;
        for (int i = 0; i < gen::bus::kBusIssueCount; ++i) {
            const auto kind = static_cast<gen::bus::BusIssue>(i);
            issues[gen::bus::toString(kind)] = m.bus().issueCount(kind);
        }
        j["busIssues"] = issues;
        std::printf("%s\n", j.dump(2).c_str());
    } else {
        std::printf("run: %s\n", lm.title.c_str());
        std::printf("frames %llu, instructions %llu, %.1f ms (%.3f ms/frame)\n",
                    static_cast<unsigned long long>(m.frame()),
                    static_cast<unsigned long long>(r.instructions), ms, msPerFrame);
        std::printf("state hash %s, PC $%06X, V-ints %llu (last from PC $%06X)\n", hash.c_str(), cpu.pc,
                    static_cast<unsigned long long>(st.vints), st.lastVintPc);
        if (st.faults != 0) {
            std::printf("faults %llu: first vector %d at PC $%06X\n",
                        static_cast<unsigned long long>(st.faults), st.firstFaultVector, st.firstFaultPc);
        } else {
            std::printf("faults none\n");
        }
        for (int i = 0; i < gen::bus::kBusIssueCount; ++i) {
            const auto kind = static_cast<gen::bus::BusIssue>(i);
            if (const auto n = m.bus().issueCount(kind); n != 0) {
                std::printf("bus %s: %llu\n", gen::bus::toString(kind), static_cast<unsigned long long>(n));
            }
        }
        const auto& issues = m.bus().issues();
        for (std::size_t i = 0; i < issues.size() && i < 8; ++i) {
            const auto& e = issues[i];
            std::printf("  %s %s.%c $%06X = $%X (PC $%06X)\n", gen::bus::toString(e.issue),
                        e.write ? "write" : "read", e.size == 1 ? 'b' : 'w', e.addr, e.value, e.pc);
        }
    }
    return st.faults != 0 || cpu.halted ? kExitCheckFailure : kExitOk;
}

int cmdTrace(const Args& a) {
    std::uint64_t frames = 1;
    std::uint64_t startFrame = 0;
    if (!parseU64(a.frames, frames) || !parseU64(a.startFrame, startFrame)) {
        return usageError("--frames and --start-frame expect non-negative integers");
    }
    gen::instrument::TraceConfig config;
    config.startFrame = startFrame;
    config.endFrame = frames;
    config.firstExecutionOnly = a.firstOnly;
    if (!parseU64(a.maxRecords, config.maxRecords)) {
        return usageError("--max-records expects a non-negative integer");
    }
    if (a.pcRange) {
        const auto dash = a.pcRange->find('-');
        std::uint64_t lo = 0;
        std::uint64_t hi = 0;
        if (dash == std::string_view::npos || !parseU64(a.pcRange->substr(0, dash), lo, 16) ||
            !parseU64(a.pcRange->substr(dash + 1), hi, 16) || lo > hi) {
            return usageError("--pc expects <lo>-<hi> in hex, e.g. 200-3FF");
        }
        config.pcRange = std::pair{static_cast<std::uint32_t>(lo), static_cast<std::uint32_t>(hi)};
    }
    LoadedMachine lm;
    if (const int rc = loadMachine(a, lm); rc != kExitOk) {
        return rc;
    }
    std::ofstream file;
    std::ostream* out = &std::cout;
    if (a.out) {
        file.open(std::filesystem::path(std::string(*a.out)), std::ios::binary);
        out = &file;
        if (!file) {
            std::fprintf(stderr, "gen_cli: cannot write '%.*s'\n", static_cast<int>(a.out->size()),
                         a.out->data());
            return kExitLoadError;
        }
    }
    std::uint64_t records = 0;
    {
        gen::instrument::TraceWriter writer(
            *lm.machine, out, config,
            [](std::span<const std::uint8_t> bytes, std::uint32_t addr, int& length) {
                const auto ins = gen::analysis::disasm68k::disassemble(bytes, addr);
                length = ins.length;
                return ins.text();
            });
        writer.attach();
        lm.machine->run(gen::RunLimit::frames(frames));
        writer.finish();
        records = writer.records();
    }
    if (a.out) {
        file.close();
        std::printf("trace: %llu records, frames %llu-%llu, state hash %s\n",
                    static_cast<unsigned long long>(records), static_cast<unsigned long long>(startFrame),
                    static_cast<unsigned long long>(frames == 0 ? 0 : frames - 1),
                    gen::state::hashToHex(lm.machine->stateHash()).c_str());
    }
    return lm.machine->stats().faults != 0 ? kExitCheckFailure : kExitOk;
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
    if (command == "trace") {
        return cmdTrace(a);
    }
    return usageError("unknown command '" + std::string(command) + "'");
}
