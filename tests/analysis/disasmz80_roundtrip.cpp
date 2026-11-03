// Z80 disassembler round trip through The Macro Assembler AS (story M04-06).
//
//   gen_disasmz80_roundtrip [--asl PATH] [--work DIR]
//
// Builds a corpus of every opcode in every prefix group (unprefixed, CB, ED, DD, FD, DDCB, FDCB), each with
// two sets of operand bytes, disassembles it, assembles the listing with AS (cpu z80undoc) and byte-compares
// the result. Encodings emitted as db take part too and must round-trip as data. Exit codes: 0 identical,
// 1 mismatch or assembler error, 4 AS not available (CTest "skipped").

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

#include "analysis/disasmz80/disasmz80.hpp"
#include "common/paths.hpp"

namespace fs = std::filesystem;
namespace dz = gen::analysis::disasmz80;

namespace {

constexpr std::uint16_t kOrigin = 0x8000;  // relative jumps never wrap around $0000

fs::path findAsl(const fs::path& explicitPath) {
    if (!explicitPath.empty()) {
        return explicitPath;
    }
    if (const auto env = gen::common::getEnvPath("GENESIS_ASL")) {
        return *env;
    }
    for (const char* rel : {"test-data/as/windows/bin/asl.exe", "test-data/as/bin/asl"}) {
        const fs::path p = fs::path(GENESIS_SOURCE_DIR) / rel;
        if (fs::exists(p)) {
            return p;
        }
    }
    return {};
}

int run(const std::string& command) {
#ifdef _WIN32
    return std::system(("\"" + command + "\"").c_str());  // cmd.exe strips the outer quotes
#else
    return std::system(command.c_str());
#endif
}

std::string quoted(const fs::path& p) {
    return "\"" + p.string() + "\"";
}

}  // namespace

int main(int argc, char** argv) {
    fs::path aslPath;
    fs::path work = fs::temp_directory_path() / "genesisEmu-disasmz80-roundtrip";
    for (int i = 1; i < argc; ++i) {
        const std::string_view a = argv[i];
        if (a == "--asl" && i + 1 < argc) {
            aslPath = argv[++i];
        } else if (a == "--work" && i + 1 < argc) {
            work = argv[++i];
        } else {
            std::fprintf(stderr, "unknown option %s\n", argv[i]);
            return 2;
        }
    }
    const fs::path asl = findAsl(aslPath);
    if (asl.empty() || !fs::exists(asl)) {
        std::printf("AS not found; run: python tools/fetch-test-data.py --only as (or set GENESIS_ASL)\n");
        return 4;
    }
    const fs::path p2bin = asl.parent_path() / asl.filename().string().replace(0, 3, "p2bin");
    fs::create_directories(work);

    // Prefix groups; the opcode byte follows the prefix (DDCB/FDCB: after the displacement).
    struct Group {
        std::vector<std::uint8_t> prefix;
        bool displacementFirst;
    };
    const std::vector<Group> groups = {{{}, false},         {{0xCB}, false}, {{0xED}, false},
                                       {{0xDD}, false},     {{0xFD}, false}, {{0xDD, 0xCB}, true},
                                       {{0xFD, 0xCB}, true}};
    const std::array<std::array<std::uint8_t, 3>, 2> operandSets{{{0x12, 0x34, 0x56}, {0x85, 0xFE, 0x7F}}};

    std::vector<std::uint8_t> expected;
    std::string source = "\tcpu z80undoc\n\trelaxed on\n\torg " + std::to_string(kOrigin) + "\n";
    std::size_t instructions = 0;
    std::size_t dataItems = 0;
    for (const auto& ops : operandSets) {
        for (const auto& g : groups) {
            for (int op = 0; op < 256; ++op) {
                std::vector<std::uint8_t> buf = g.prefix;
                if (g.displacementFirst) {
                    buf.push_back(ops[0]);
                    buf.push_back(static_cast<std::uint8_t>(op));
                } else {
                    buf.push_back(static_cast<std::uint8_t>(op));
                    buf.insert(buf.end(), ops.begin(), ops.end());
                }
                const auto address = static_cast<std::uint16_t>(kOrigin + expected.size());
                const auto ins = dz::disassemble(buf, address);
                expected.insert(expected.end(), buf.begin(), buf.begin() + ins.length);
                source += "\t" + ins.mnemonic + (ins.operands.empty() ? "" : "\t" + ins.operands) + "\n";
                (ins.valid() ? instructions : dataItems) += 1;
            }
        }
    }
    source += "\tend\n";
    const fs::path asmFile = work / "corpus.asm";
    const fs::path pFile = work / "corpus.p";
    const fs::path binFile = work / "corpus.bin";
    const fs::path logFile = work / "asl.log";
    {
        std::ofstream out(asmFile, std::ios::binary);
        out << source;
    }
    std::printf("corpus: %zu items (%zu instructions, %zu db), %zu bytes\n", instructions + dataItems,
                instructions, dataItems, expected.size());
    fs::remove(pFile);
    fs::remove(binFile);
    if (run(quoted(asl) + " -q -xx -L -o " + quoted(pFile) + " " + quoted(asmFile) + " > " + quoted(logFile) +
            " 2>&1") != 0 ||
        !fs::exists(pFile)) {
        std::printf("AS reported errors (first lines of %s):\n", logFile.string().c_str());
        std::ifstream log(logFile);
        std::string line;
        for (int shown = 0; std::getline(log, line) && shown < 20; ++shown) {
            std::printf("  %s\n", line.c_str());
        }
        return 1;
    }
    if (run(quoted(p2bin) + " " + quoted(pFile) + " " + quoted(binFile) + " > " + quoted(logFile) +
            " 2>&1") != 0) {
        std::printf("p2bin failed\n");
        return 1;
    }
    std::ifstream in(binFile, std::ios::binary);
    const std::vector<std::uint8_t> actual((std::istreambuf_iterator<char>(in)),
                                           std::istreambuf_iterator<char>());
    std::size_t mismatches = 0;
    for (std::size_t i = 0; i < expected.size(); ++i) {
        if (i >= actual.size() || actual[i] != expected[i]) {
            if (mismatches++ < 10) {
                std::printf("mismatch at $%04zX: expected $%02X, got $%02X\n", kOrigin + i, expected[i],
                            i < actual.size() ? actual[i] : 0);
            }
        }
    }
    if (actual.size() != expected.size()) {
        std::printf("size: expected %zu bytes, got %zu\n", expected.size(), actual.size());
        ++mismatches;
    }
    std::printf("%s: %zu mismatching bytes\n", mismatches == 0 ? "identical" : "MISMATCH", mismatches);
    return mismatches == 0 ? 0 : 1;
}
