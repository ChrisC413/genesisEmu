// ZEXDOC / ZEXALL runner (story M04-04): runs a CP/M .com exerciser headless through a minimal BDOS shim.
//
//   gen_z80_zex <zexdoc.com|zexall.com|NAME> [--quiet]
//
// A bare NAME (zexdoc, zexall) is looked up in test-data/zex. The program loads at $0100; BDOS calls at
// $0005 support C=2 (print E) and C=9 (print the '$'-terminated string at DE); a jump to $0000 ends the run.
// Prints the program output and a summary line "<name>: <ok>/<total> tests OK". Exit codes: 0 every test
// OK, 1 a test reported ERROR (or the run did not finish), 2 usage error, 4 program not found (skipped).

#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

#include "core/z80/z80.hpp"

namespace fs = std::filesystem;

namespace {

class CpmBus final : public gen::z80::BusZ80 {
public:
    std::array<std::uint8_t, 0x10000> mem{};
    std::uint8_t read8(std::uint16_t addr) override { return mem[addr]; }
    void write8(std::uint16_t addr, std::uint8_t v) override { mem[addr] = v; }
};

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: gen_z80_zex <file.com|zexdoc|zexall> [--quiet]\n");
        return 2;
    }
    fs::path path = argv[1];
    const bool quiet = argc > 2 && std::string_view(argv[2]) == "--quiet";
    if (path.extension() != ".com") {
        path = fs::path(GENESIS_SOURCE_DIR) / "test-data" / "zex" / (path.string() + ".com");
    }
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        std::printf("%s not found; run: python tools/fetch-test-data.py --only zex\n", path.string().c_str());
        return 4;
    }
    const std::vector<char> program((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());

    CpmBus bus;
    for (std::size_t i = 0; i < program.size() && 0x100 + i < bus.mem.size(); ++i) {
        bus.mem[0x100 + i] = static_cast<std::uint8_t>(program[i]);
    }
    bus.mem[0x0000] = 0x76;  // warm boot: HALT (detected below)
    bus.mem[0x0005] = 0xC3;  // BDOS entry: JP $F000 (the address at $0006 is the top of the TPA)
    bus.mem[0x0006] = 0x00;
    bus.mem[0x0007] = 0xF0;
    bus.mem[0xF000] = 0xC9;  // RET

    gen::z80::Z80 cpu(bus);
    cpu.reset();
    auto& s = cpu.state();
    s.pc = 0x0100;
    s.sp = 0xF000;

    std::string output;
    std::string line;
    int ok = 0;
    int errors = 0;
    std::uint64_t instructions = 0;
    std::uint64_t tstates = 0;
    const auto t0 = std::chrono::steady_clock::now();
    const auto emit = [&](char ch) {
        output += ch;
        if (!quiet) {
            std::fputc(ch, stdout);
        }
        if (ch == '\n') {
            if (line.find("OK") != std::string::npos) {
                ++ok;
            }
            if (line.find("ERROR") != std::string::npos) {
                ++errors;
            }
            line.clear();
            std::fflush(stdout);
        } else if (ch != '\r') {
            line += ch;
        }
    };
    bool finished = false;
    while (!finished) {
        if (s.pc == 0x0005) {
            if (s.c == 2) {
                emit(static_cast<char>(s.e));
            } else if (s.c == 9) {
                for (std::uint16_t a = s.de(); bus.mem[a] != '$'; ++a) {
                    emit(static_cast<char>(bus.mem[a]));
                }
            }
        } else if (s.pc == 0x0000) {
            finished = true;
            break;
        }
        tstates += static_cast<std::uint64_t>(cpu.step());
        ++instructions;
        if (s.halted) {
            break;  // HALT outside the warm-boot vector: the program crashed
        }
    }
    const double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    if (!line.empty()) {
        emit('\n');
    }
    const int total = ok + errors;
    std::printf("%s: %d/%d tests OK, %d ERROR, %llu instructions, %llu T-states, %.1f s (%.0f MIPS)%s\n",
                path.stem().string().c_str(), ok, total, errors, static_cast<unsigned long long>(instructions),
                static_cast<unsigned long long>(tstates), secs,
                secs > 0 ? static_cast<double>(instructions) / secs / 1e6 : 0.0, finished ? "" : " (did not finish)");
    return finished && errors == 0 && ok > 0 ? 0 : 1;
}
