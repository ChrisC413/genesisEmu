// gen_cli: headless runner for automation and CI.

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

#include "analysis/disasm68k/disasm68k.hpp"
#include "common/golden_frames.hpp"
#include "common/input_script.hpp"
#include "common/logging.hpp"
#include "common/paths.hpp"
#include "common/png.hpp"
#include "common/rom_info.hpp"
#include "common/sram_file.hpp"
#include "common/wav.hpp"
#include "core/cart/rom.hpp"
#include "core/sched/machine.hpp"
#include "core/state/save_state.hpp"
#include "core/state/state_hash.hpp"
#include "core/util/hash.hpp"
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
      --dump-frames <list>  comma-separated frame numbers; after each of those
                        frames the framebuffer is written as PNG to --out
      --out <dir>       directory for --dump-frames (default: current directory)
      --check <file>    golden frame checkpoints (tests/golden/m03_frames.json):
                        compare the frame hash at each checkpoint of this ROM;
                        mismatches print, write PNGs to --out and exit 3
      --dump-vdp        with --dump-frames, also write vdp_<N>.bin (registers,
                        VRAM, CRAM, VSRAM) for offline inspection
      --reg-log <file>  write the sound-chip register write log (YM2612 / PSG,
                        last 65536 writes) as CSV
      --input <file>    scripted input (spec 13): button lines and checkpoints;
                        runs at least to the script's last frame and prints
                        every checkpoint hash
      --check           (with --input, no value) fail with exit 3 when a
                        checkpoint mismatches or has no expected hash
      --update-script   (with --input) write the computed hashes into the script
      --sram <file>     load battery SRAM from <file> before the run (if it
                        exists) and save it there afterwards
      --pad <3|6>       controller type of both ports (default 3)
      --wav <file>      record the mixed audio (16-bit stereo at the native
                        ~53,267 Hz rate) of the whole run
      --load-state <file>  start from a save state (spec 08) instead of power
                        on; the state must come from the same ROM image
      --save-state-at <frame>:<file>  after <frame> frames, write a save
                        state to <file> (repeatable); e.g. record once with
                        --input and start later runs right before a level

  trace --rom <file> [--lock-on <file>] [--load-state <file>] [--frames <N>]
        [--start-frame <S>] [--pc <lo>-<hi>] [--first] [--max-records <N>]
        [--out <file>]
      Run N frames (default 1) and write a text instruction trace (format in
      docs/specs/08-debugger.md) of the frames from S on to --out (default
      stdout).
      --pc           only log instructions whose PC is in [lo, hi] (hex)
      --first        log each PC only the first time it executes
      --max-records  stop logging after N records

  help [command]
      Show this help.

Planned: coverage, export.

Frame numbers: --frames is how many frames to run (from power on, or from the
frame a --load-state state was saved at); every other frame number
(--dump-frames, --check, --input, --save-state-at, --start-frame) counts from
power on. Options can also be written --name=value.

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
    bool dumpVdp = false;
    bool firstOnly = false;
    std::optional<std::string_view> maxRecords;
    std::optional<std::string_view> dumpFrames;
    std::optional<std::string_view> check;
    std::optional<std::string_view> regLog;
    std::optional<std::string_view> input;
    std::optional<std::string_view> sram;
    std::optional<std::string_view> pad;
    std::optional<std::string_view> wav;
    std::optional<std::string_view> loadState;
    std::vector<std::string_view> saveStateAt;
    bool checkScript = false;
    bool updateScript = false;
    bool help = false;
    bool version = false;
    std::string error;
};

Args parseArgs(int argc, char** argv) {
    Args a;
    for (int i = 1; i < argc; ++i) {
        std::string_view arg = argv[i];
        // --name=value is the same as --name value.
        std::optional<std::string_view> inlineValue;
        if (arg.starts_with("--")) {
            if (const auto eq = arg.find('='); eq != std::string_view::npos) {
                inlineValue = arg.substr(eq + 1);
                arg = arg.substr(0, eq);
            }
        }
        bool tookValue = false;
        const auto value = [&](std::optional<std::string_view>& out) {
            tookValue = true;
            if (inlineValue) {
                out = *inlineValue;
                return;
            }
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
        } else if (arg == "--check") {
            if (!inlineValue && (i + 1 >= argc || std::string_view(argv[i + 1]).starts_with("--"))) {
                a.checkScript = true;  // scripted-input checkpoints
            } else {
                value(a.check);
            }
        } else if (arg == "--input") {
            value(a.input);
        } else if (arg == "--sram") {
            value(a.sram);
        } else if (arg == "--wav") {
            value(a.wav);
        } else if (arg == "--pad") {
            value(a.pad);
        } else if (arg == "--load-state") {
            value(a.loadState);
        } else if (arg == "--save-state-at") {
            std::optional<std::string_view> v;
            value(v);
            if (v) {
                a.saveStateAt.push_back(*v);
            }
        } else if (arg == "--update-script") {
            a.updateScript = true;
        } else if (arg == "--dump-frames") {
            value(a.dumpFrames);
        } else if (arg == "--reg-log") {
            value(a.regLog);
        } else if (arg == "--dump-vdp") {
            a.dumpVdp = true;
        } else if (arg == "--frame-log") {
            a.frameLog = true;
        } else if (arg == "--json") {
            a.json = true;
        } else if (arg.starts_with("-")) {
            a.error = "unknown option " + std::string(arg);
        } else {
            a.positional.push_back(arg);
        }
        if (a.error.empty() && inlineValue && !tookValue) {
            a.error = "option " + std::string(arg) + " takes no value";
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
    std::string sha1;  // of the loaded image (lock-on: the combined image)
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
    out.sha1 = gen::toHex(gen::sha1(image));
    out.machine = std::make_unique<gen::Machine>(std::move(image), regions, config);
    if (a.loadState) {
        std::ifstream in(gen::common::fromUtf8(*a.loadState), std::ios::binary);
        const std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(in)),
                                              std::istreambuf_iterator<char>());
        std::string error;
        if (bytes.empty()) {
            error = "cannot read the file (missing or empty)";
        }
        if (!error.empty() || !out.machine->loadState(bytes, error)) {
            std::fprintf(stderr, "gen_cli: --load-state %s: %s\n", std::string(*a.loadState).c_str(),
                         error.c_str());
            return kExitLoadError;
        }
        std::printf("state: loaded %s (frame %llu)\n", std::string(*a.loadState).c_str(),
                    static_cast<unsigned long long>(out.machine->frame()));
    }
    return kExitOk;
}

struct StateSave {
    std::uint64_t frame = 0;
    std::filesystem::path path;
};

// Parses the --save-state-at <frame>:<file> values; frames must lie after `start`.
bool parseStateSaves(const Args& a, std::uint64_t start, std::vector<StateSave>& out, std::string& error) {
    for (const std::string_view v : a.saveStateAt) {
        const auto colon = v.find(':');
        StateSave save;
        if (colon == std::string_view::npos || colon + 1 >= v.size() ||
            !parseU64(v.substr(0, colon), save.frame)) {
            error = "--save-state-at expects <frame>:<file>, e.g. 1500:cpz.gst";
            return false;
        }
        if (save.frame <= start) {
            error = "--save-state-at frame " + std::to_string(save.frame) + " is not after the start frame " +
                    std::to_string(start);
            return false;
        }
        save.path = gen::common::fromUtf8(v.substr(colon + 1));
        out.push_back(std::move(save));
    }
    return true;
}

bool writeState(gen::Machine& m, const std::filesystem::path& path) {
    const auto bytes = m.saveState(static_cast<std::uint64_t>(std::time(nullptr)), true);  // with a thumbnail
    std::ofstream f(path, std::ios::binary);
    f.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!f) {
        std::fprintf(stderr, "gen_cli: cannot write %s\n", gen::common::toUtf8(path).c_str());
        return false;
    }
    std::printf("state: saved frame %llu to %s\n", static_cast<unsigned long long>(m.frame()),
                gen::common::toUtf8(path).c_str());
    return true;
}

// Raw VDP memories next to a dumped frame: 24 registers, 64 KB VRAM, CRAM and VSRAM as big-endian words.
void writeVdpDump(const std::filesystem::path& path, const gen::vdp::VdpState& s) {
    std::vector<std::uint8_t> out(s.regs.begin(), s.regs.end());
    out.insert(out.end(), s.vram.begin(), s.vram.end());
    for (const std::uint16_t w : s.cram) {
        out.push_back(static_cast<std::uint8_t>(w >> 8));
        out.push_back(static_cast<std::uint8_t>(w));
    }
    for (const std::uint16_t w : s.vsram) {
        out.push_back(static_cast<std::uint8_t>(w >> 8));
        out.push_back(static_cast<std::uint8_t>(w));
    }
    std::ofstream f(path, std::ios::binary);
    f.write(reinterpret_cast<const char*>(out.data()), static_cast<std::streamsize>(out.size()));
}

struct DmaCounts {
    std::array<std::uint64_t, 3> count{};
    std::uint64_t words = 0;
    static void onDma(void* ctx, const gen::DmaEvent& e) {
        auto& self = *static_cast<DmaCounts*>(ctx);
        ++self.count[static_cast<std::size_t>(e.kind)];
        if (e.kind == gen::vdp::DmaKind::MemoryToVdp) {
            self.words += e.length;
        }
    }
};

int cmdRun(const Args& a) {
    std::uint64_t frames = 1;
    if (!parseU64(a.frames, frames)) {
        return usageError("--frames expects a non-negative integer");
    }
    LoadedMachine lm;
    if (const int rc = loadMachine(a, lm); rc != kExitOk) {
        return rc;
    }
    const std::uint64_t startFrame = lm.machine->frame();
    std::uint64_t endFrame = startFrame + frames;
    std::vector<StateSave> stateSaves;
    if (std::string error; !parseStateSaves(a, startFrame, stateSaves, error)) {
        return usageError(error);
    }
    for (const auto& s : stateSaves) {
        endFrame = std::max(endFrame, s.frame);
    }
    std::vector<std::uint64_t> dumps;
    if (a.dumpFrames) {
        std::string_view rest = *a.dumpFrames;
        while (!rest.empty()) {
            const auto comma = rest.find(',');
            std::uint64_t n = 0;
            if (!parseU64(rest.substr(0, comma), n) || n <= startFrame) {
                return usageError(
                    "--dump-frames expects comma-separated frame numbers after the start frame");
            }
            dumps.push_back(n);
            rest = comma == std::string_view::npos ? std::string_view{} : rest.substr(comma + 1);
        }
        if (!dumps.empty()) {
            endFrame = std::max(endFrame, *std::max_element(dumps.begin(), dumps.end()));
        }
    }
    const std::filesystem::path outDir = a.out ? gen::common::fromUtf8(*a.out) : std::filesystem::path(".");
    gen::Machine& m = *lm.machine;
    if (a.pad) {
        if (*a.pad != "3" && *a.pad != "6") {
            return usageError("--pad expects 3 or 6");
        }
        const auto type = *a.pad == "6" ? gen::bus::PadType::SixButton : gen::bus::PadType::ThreeButton;
        m.setPadType(0, type);
        m.setPadType(1, type);
    }
    if (a.sram && m.sram() != nullptr && gen::common::loadSram(*m.sram(), gen::common::fromUtf8(*a.sram))) {
        std::printf("sram: loaded %s\n", std::string(*a.sram).c_str());
    }
    std::optional<gen::common::InputScript> script;
    std::string scriptText;
    if (a.input) {
        std::ifstream in(gen::common::fromUtf8(*a.input), std::ios::binary);
        if (!in) {
            std::fprintf(stderr, "gen_cli: cannot read %s\n", std::string(*a.input).c_str());
            return kExitLoadError;
        }
        scriptText.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
        std::string error;
        script = gen::common::parseInputScript(scriptText, error);
        if (!script) {
            std::fprintf(stderr, "gen_cli: %s: %s\n", std::string(*a.input).c_str(), error.c_str());
            return kExitUsage;
        }
        endFrame = std::max(endFrame, script->lastFrame());
        if (!script->romSha1.empty() && script->romSha1 != lm.sha1) {
            std::fprintf(stderr, "gen_cli: warning: the script is for SHA-1 %s, the image is %s\n",
                         script->romSha1.c_str(), lm.sha1.c_str());
        }
    }
    std::optional<gen::common::InputPlayer> player;
    if (script) {
        player.emplace(*script);
    }
    gen::common::WavWriter wav;
    if (a.wav && !wav.open(gen::common::fromUtf8(*a.wav), m.sound().sampleRate())) {
        std::fprintf(stderr, "gen_cli: cannot write %s\n", std::string(*a.wav).c_str());
        return kExitLoadError;
    }
    std::map<std::size_t, std::string> scriptHashes;
    int scriptFailures = 0;
    DmaCounts dma;
    gen::Hooks hooks;
    hooks.ctx = &dma;
    hooks.onDma = &DmaCounts::onDma;
    m.setHooks(hooks);
    const auto t0 = std::chrono::steady_clock::now();
    gen::RunResult r;
    // --check: golden frame checkpoints for this image (M03-11).
    std::vector<gen::common::FrameCheckpoint> checks;
    std::string checkName;
    if (a.check) {
        try {
            for (const auto& g : gen::common::loadFrameGolden(gen::common::fromUtf8(*a.check))) {
                if (g.sha1 == lm.sha1) {
                    checks = g.checkpoints;
                    checkName = g.name;
                }
            }
        } catch (const std::exception& e) {
            std::fprintf(stderr, "gen_cli: --check: %s\n", e.what());
            return kExitLoadError;
        }
        if (checks.empty()) {
            std::fprintf(stderr, "gen_cli: --check: no checkpoints for SHA-1 %s\n", lm.sha1.c_str());
            return kExitCheckFailure;
        }
        for (const auto& c : checks) {
            endFrame = std::max(endFrame, c.frame);
        }
    }
    int mismatches = 0;
    if (a.frameLog || !dumps.empty() || !checks.empty() || script || a.wav || !stateSaves.empty()) {
        while (m.frame() < endFrame) {
            const std::uint64_t f = m.frame();
            if (player) {
                player->beforeFrame(m);
            }
            r.instructions += m.run(gen::RunLimit::frames(1)).instructions;
            wav.write(m.sound().samples());
            m.sound().clearSamples();
            if (script) {
                for (const auto& c : script->checkpoints) {
                    if (c.frame <= f || c.frame > m.frame()) {
                        continue;  // a DMA can carry one run past a frame boundary
                    }
                    std::string got;
                    if (c.kind == gen::common::CheckKind::FrameHash) {
                        got = gen::state::hashToHex(m.vdp().frameHash());
                    } else if (c.kind == gen::common::CheckKind::StateHash) {
                        got = gen::state::hashToHex(m.stateHash());
                    } else {
                        got = gen::state::hashToHex(m.sound().audioHash());
                    }
                    scriptHashes[c.line] = got;
                    const char* verdict = !c.expected          ? "(no expected hash)"
                                          : *c.expected == got ? "ok"
                                                               : "MISMATCH";
                    if (a.checkScript && (!c.expected || *c.expected != got)) {
                        ++scriptFailures;
                    }
                    std::printf("checkpoint %llu %s %s %s\n", static_cast<unsigned long long>(c.frame),
                                gen::common::toString(c.kind), got.c_str(), verdict);
                    if (c.expected && *c.expected != got && c.kind == gen::common::CheckKind::FrameHash) {
                        const auto& v = m.vdp();
                        const auto path = outDir / ("checkpoint_" + std::to_string(c.frame) + ".png");
                        gen::common::writePng(path, v.framebuffer(), v.frameWidth(), v.frameHeight(),
                                              gen::vdp::kFbWidth);
                    }
                }
            }
            if (a.frameLog) {
                std::printf("frame %llu: V-ints %u, last V-int from PC $%06X, PC $%06X, frame hash %s\n",
                            static_cast<unsigned long long>(f), m.stats().vintsLastFrame,
                            m.stats().lastVintPc, m.cpu().state().pc,
                            gen::state::hashToHex(m.vdp().frameHash()).c_str());
            }
            for (const auto& c : checks) {
                if (c.frame <= f || c.frame > m.frame()) {
                    continue;
                }
                const std::string got = gen::state::hashToHex(m.vdp().frameHash());
                if (got == c.hash) {
                    std::printf("check %s frame %llu: ok\n", checkName.c_str(),
                                static_cast<unsigned long long>(c.frame));
                    gen::common::cacheGoldenFrame(lm.sha1, c, m.vdp());
                    continue;
                }
                ++mismatches;
                std::printf("check %s frame %llu: MISMATCH expected %s, got %s\n", checkName.c_str(),
                            static_cast<unsigned long long>(c.frame), c.hash.c_str(), got.c_str());
                const auto stem = lm.sha1.substr(0, 8) + "_frame_" + std::to_string(c.frame);
                for (const auto& file : gen::common::writeFrameMismatch(outDir, stem, lm.sha1, c, m.vdp())) {
                    std::printf("  wrote %s\n", gen::common::toUtf8(file).c_str());
                }
            }
            for (const auto& s : stateSaves) {
                if (s.frame > f && s.frame <= m.frame() && !writeState(m, s.path)) {
                    return kExitLoadError;
                }
            }
            if (std::any_of(dumps.begin(), dumps.end(),
                            [&](std::uint64_t n) { return n > f && n <= m.frame(); })) {
                const auto& v = m.vdp();
                const auto path = outDir / ("frame_" + std::to_string(m.frame()) + ".png");
                if (!gen::common::writePng(path, v.framebuffer(), v.frameWidth(), v.frameHeight(),
                                           gen::vdp::kFbWidth)) {
                    std::fprintf(stderr, "gen_cli: cannot write %s\n", gen::common::toUtf8(path).c_str());
                    return kExitLoadError;
                }
                if (a.dumpVdp) {
                    writeVdpDump(outDir / ("vdp_" + std::to_string(m.frame()) + ".bin"), v.state());
                }
            }
        }
    } else {
        r = m.run(gen::RunLimit::frames(endFrame - startFrame));
    }
    const std::uint64_t framesRun = m.frame() - startFrame;
    const double ms =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    const auto& st = m.stats();
    const auto& cpu = m.cpu().state();
    const std::string hash = gen::state::hashToHex(m.stateHash());
    const double msPerFrame = framesRun != 0 ? ms / static_cast<double>(framesRun) : 0.0;
    if (a.json) {
        nlohmann::json j;
        j["title"] = lm.title;
        j["frames"] = m.frame();
        j["instructions"] = r.instructions;
        j["ms"] = ms;
        j["msPerFrame"] = msPerFrame;
        j["stateHash"] = hash;
        j["frameHash"] = gen::state::hashToHex(m.vdp().frameHash());
        j["dma"] = {{"memoryToVdp", dma.count[0]},
                    {"words", dma.words},
                    {"fill", dma.count[1]},
                    {"copy", dma.count[2]}};
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
        j["soundWrites"] = m.z80().log().total();
        j["z80Pc"] = m.z80Cpu().state().pc;
        std::printf("%s\n", j.dump(2).c_str());
    } else {
        std::printf("run: %s\n", lm.title.c_str());
        std::printf("frames %llu, instructions %llu, %.1f ms (%.3f ms/frame)\n",
                    static_cast<unsigned long long>(m.frame()),
                    static_cast<unsigned long long>(r.instructions), ms, msPerFrame);
        std::printf("state hash %s, PC $%06X, V-ints %llu (last from PC $%06X)\n", hash.c_str(), cpu.pc,
                    static_cast<unsigned long long>(st.vints), st.lastVintPc);
        std::printf("frame hash %s (%dx%d)\n", gen::state::hashToHex(m.vdp().frameHash()).c_str(),
                    m.vdp().frameWidth(), m.vdp().frameHeight());
        std::printf("DMA: %llu 68k->VDP (%llu words), %llu fill, %llu copy\n",
                    static_cast<unsigned long long>(dma.count[0]), static_cast<unsigned long long>(dma.words),
                    static_cast<unsigned long long>(dma.count[1]),
                    static_cast<unsigned long long>(dma.count[2]));
        std::printf("Z80 PC $%04X, sound-chip writes %llu\n", m.z80Cpu().state().pc,
                    static_cast<unsigned long long>(m.z80().log().total()));
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
    if (wav.isOpen()) {
        std::printf("wav: %llu frames at %.0f Hz -> %s\n", static_cast<unsigned long long>(wav.frames()),
                    m.sound().sampleRate(), std::string(*a.wav).c_str());
        wav.close();
    }
    if (a.sram && m.sram() != nullptr) {
        if (!gen::common::saveSram(*m.sram(), gen::common::fromUtf8(*a.sram))) {
            std::fprintf(stderr, "gen_cli: cannot write %s\n", std::string(*a.sram).c_str());
            return kExitLoadError;
        }
    }
    if (a.updateScript && script) {
        std::ofstream outScript(gen::common::fromUtf8(*a.input), std::ios::binary);
        outScript << gen::common::updateScriptHashes(scriptText, scriptHashes);
        std::printf("updated %zu checkpoint hash(es) in %s\n", scriptHashes.size(),
                    std::string(*a.input).c_str());
    }
    if (scriptFailures != 0) {
        std::printf("%d scripted checkpoint failure(s)\n", scriptFailures);
        mismatches += scriptFailures;
    }
    if (a.regLog) {
        std::ofstream csv(gen::common::fromUtf8(*a.regLog), std::ios::binary);
        csv << m.z80().log().toCsv();
        if (!csv) {
            std::fprintf(stderr, "gen_cli: cannot write %.*s\n", static_cast<int>(a.regLog->size()),
                         a.regLog->data());
            return kExitLoadError;
        }
    }
    if (mismatches != 0) {
        std::printf("%d golden frame mismatch(es)\n", mismatches);
    }
    return st.faults != 0 || cpu.halted || mismatches != 0 ? kExitCheckFailure : kExitOk;
}

int cmdTrace(const Args& a) {
    std::uint64_t frames = 1;
    std::uint64_t startFrame = 0;
    if (!parseU64(a.frames, frames) || !parseU64(a.startFrame, startFrame)) {
        return usageError("--frames and --start-frame expect non-negative integers");
    }
    gen::instrument::TraceConfig config;
    config.startFrame = startFrame;
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
    const std::uint64_t base = lm.machine->frame();
    config.endFrame = base + frames;
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
                    static_cast<unsigned long long>(records),
                    static_cast<unsigned long long>(std::max(startFrame, base)),
                    static_cast<unsigned long long>(frames == 0 ? base : base + frames - 1),
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
