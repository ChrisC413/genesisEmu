// Disassembler round trip through The Macro Assembler AS (story M01-12).
//
//   gen_disasm_roundtrip [--asl PATH] [--work DIR] [--keep]
//
// Builds a corpus of every 65,536 first words, each with three sets of representative extension words,
// disassembles it, assembles the listing with AS and byte-compares the result with the corpus. Instructions
// the disassembler emits as dc.w (invalid or non-canonical encodings) take part too and must round-trip as
// data. AS is found via --asl, GENESIS_ASL, or test-data/as/windows/bin/asl.exe (tools/fetch-test-data.py).
// Exit codes: 0 identical, 1 mismatch or assembler error, 4 AS not available (CTest "skipped").

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

#include "analysis/disasm68k/disasm68k.hpp"
#include "common/paths.hpp"

namespace fs = std::filesystem;
namespace d68 = gen::analysis::disasm68k;

namespace {

constexpr std::uint32_t kOrigin = 0x10000;

// Extension-word sets: small positive values, negative displacements / high addresses, byte-sized values.
constexpr std::array<std::array<std::uint16_t, 5>, 3> kExtensionSets{{
    {0x0012, 0x0034, 0x0056, 0x0078, 0x009A},
    {0x9804, 0xF8F0, 0x8002, 0x1004, 0x2006},
    {0x00FE, 0x00FC, 0x7FFE, 0x0010, 0x0001},
}};

struct Entry {
    std::uint32_t address;
    std::string text;
};

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
    // cmd.exe strips the outer quotes of a command line that starts with a quote.
    return std::system(("\"" + command + "\"").c_str());
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
    fs::path work = fs::temp_directory_path() / "genesisEmu-disasm-roundtrip";
    bool keep = false;
    for (int i = 1; i < argc; ++i) {
        const std::string_view a = argv[i];
        if (a == "--asl" && i + 1 < argc) {
            aslPath = argv[++i];
        } else if (a == "--work" && i + 1 < argc) {
            work = argv[++i];
        } else if (a == "--keep") {
            keep = true;
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

    // Corpus and listing.
    std::vector<std::uint8_t> expected;
    std::vector<Entry> entries;
    std::string source = "\tcpu 68000\n\tsupmode on\n\tpadding off\n\torg " + std::to_string(kOrigin) + "\n";
    std::size_t valid = 0;
    std::size_t dataWords = 0;
    std::vector<bool> decodedOnce(65536, false);
    for (const auto& set : kExtensionSets) {
        for (std::uint32_t op = 0; op < 65536; ++op) {
            std::array<std::uint8_t, 12> buf{};
            buf[0] = static_cast<std::uint8_t>(op >> 8);
            buf[1] = static_cast<std::uint8_t>(op);
            for (std::size_t i = 0; i < set.size(); ++i) {
                buf[2 + 2 * i] = static_cast<std::uint8_t>(set[i] >> 8);
                buf[3 + 2 * i] = static_cast<std::uint8_t>(set[i]);
            }
            const std::uint32_t address = kOrigin + static_cast<std::uint32_t>(expected.size());
            const d68::Instruction ins = d68::disassemble(buf, address);
            expected.insert(expected.end(), buf.begin(), buf.begin() + ins.length);
            entries.push_back({address, ins.text()});
            source += "\t" + ins.mnemonic + (ins.operands.empty() ? "" : "\t" + ins.operands) + "\n";
            (ins.valid() ? valid : dataWords) += 1;
            if (ins.valid()) {
                decodedOnce[op] = true;
            }
        }
    }
    // Every valid first word must be emitted as an instruction (not dc.w) with at least one extension set.
    std::size_t validFirstWords = 0;
    std::size_t neverDecoded = 0;
    for (std::uint32_t op = 0; op < 65536; ++op) {
        if (!d68::isValidFirstWord(static_cast<std::uint16_t>(op))) {
            continue;
        }
        ++validFirstWords;
        if (!decodedOnce[op] && neverDecoded++ < 10) {
            std::printf("valid opcode $%04X was only ever emitted as dc.w\n", op);
        }
    }
    std::printf("valid first words: %zu, decoded as instructions: %zu\n", validFirstWords,
                validFirstWords - neverDecoded);
    source += "\tend\n";
    const fs::path asmFile = work / "corpus.asm";
    const fs::path pFile = work / "corpus.p";
    const fs::path binFile = work / "corpus.bin";
    const fs::path logFile = work / "asl.log";
    {
        std::ofstream out(asmFile, std::ios::binary);
        out << source;
    }
    std::printf("corpus: %zu instructions (%zu decoded, %zu as dc.w), %zu bytes\n", entries.size(), valid,
                dataWords, expected.size());

    fs::remove(pFile);
    fs::remove(binFile);
    const std::string assemble = quoted(asl) + " -q -xx -o " + quoted(pFile) + " " + quoted(asmFile) + " > " +
                                 quoted(logFile) + " 2>&1";
    if (run(assemble) != 0 || !fs::exists(pFile)) {
        std::printf("AS reported errors (first lines of %s):\n", logFile.string().c_str());
        std::ifstream log(logFile);
        std::string line;
        int shown = 0;
        while (std::getline(log, line) && shown < 20) {
            if (line.find("rror") != std::string::npos || shown > 0) {
                std::printf("  %s\n", line.c_str());
                ++shown;
            }
        }
        return 1;
    }
#ifdef _WIN32
    const char* range = " -q -r $-$ ";
#else
    const char* range = " -q -r '$-$' ";
#endif
    const std::string convert =
        quoted(p2bin) + range + quoted(pFile) + " " + quoted(binFile) + " >> " + quoted(logFile) + " 2>&1";
    if (run(convert) != 0) {
        std::printf("p2bin failed; see %s\n", logFile.string().c_str());
        return 1;
    }
    std::ifstream in(binFile, std::ios::binary);
    const std::vector<std::uint8_t> actual((std::istreambuf_iterator<char>(in)),
                                           std::istreambuf_iterator<char>());

    std::size_t mismatches = 0;
    std::size_t entryIndex = 0;
    for (std::size_t i = 0; i < expected.size(); ++i) {
        if (i < actual.size() && actual[i] == expected[i]) {
            continue;
        }
        const std::uint32_t addr = kOrigin + static_cast<std::uint32_t>(i);
        while (entryIndex + 1 < entries.size() && entries[entryIndex + 1].address <= addr) {
            ++entryIndex;
        }
        if (mismatches++ < 10) {
            std::printf("mismatch at $%06X: expected %02X, got %s  [%s]\n", addr, expected[i],
                        i < actual.size() ? std::to_string(actual[i]).c_str() : "EOF",
                        entries[entryIndex].text.c_str());
        }
        // Skip the rest of this instruction.
        const std::uint32_t next = entryIndex + 1 < entries.size()
                                       ? entries[entryIndex + 1].address
                                       : kOrigin + static_cast<std::uint32_t>(expected.size());
        i = next - kOrigin - 1;
    }
    if (actual.size() != expected.size()) {
        std::printf("size differs: expected %zu bytes, got %zu\n", expected.size(), actual.size());
        ++mismatches;
    }
    if (!keep && mismatches == 0) {
        std::error_code ec;
        fs::remove_all(work, ec);
    }
    const bool pass = mismatches == 0 && neverDecoded == 0;
    std::printf("%s: %zu mismatching instructions\n", pass ? "PASS" : "FAIL", mismatches);
    return pass ? 0 : 1;
}
