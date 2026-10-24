// SingleStepTests 68000 runner (story M01-02).
//
//   gen_m68k_sst [--data DIR] [--only NAME,...] [--limit N] [--show N] [--exclusions FILE]
//                [--failures FILE] [--inject-fault] [--expect-fail]
//
// Streams each converted .sst file (tools/fetch-test-data.py), runs one instruction per vector against a flat
// 16 MB bus and compares registers, SR, prefetch queue, RAM and cycle count. Prints one line per file and a
// total. Exit codes: 0 all pass, 1 failures, 2 usage error, 4 no test data (CTest reports "skipped").

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "common/paths.hpp"
#include "core/m68k/m68k.hpp"
#include "sst_format.hpp"

namespace fs = std::filesystem;
using gen::m68k::M68k;
using gen::test::sst::CpuState;
using gen::test::sst::Vector;

namespace {

class FlatBus final : public gen::m68k::Bus68k {
public:
    FlatBus() : mem_(0x1000000, 0) {
        for (std::size_t p = 0; p < 256; ++p) {
            fastRead[p] =
                mem_.data() + (p << 16);  // reads are direct; writes stay virtual so they are tracked
        }
    }
    std::uint8_t read8(std::uint32_t addr) override { return mem_[addr]; }
    std::uint16_t read16(std::uint32_t addr) override {
        return static_cast<std::uint16_t>((mem_[addr] << 8) | mem_[(addr + 1) & 0xFFFFFF]);
    }
    void write8(std::uint32_t addr, std::uint8_t v) override {
        mem_[addr] = v;
        written_.push_back(addr);
    }
    void write16(std::uint32_t addr, std::uint16_t v) override {
        write8(addr, static_cast<std::uint8_t>(v >> 8));
        write8((addr + 1) & 0xFFFFFF, static_cast<std::uint8_t>(v));
    }
    void load(const CpuState& s) {
        for (const auto& [addr, value] : s.ram) {
            mem_[addr & 0xFFFFFF] = value;
            touched_.push_back(addr & 0xFFFFFF);
        }
        written_.clear();
    }
    void clear() {
        for (const std::uint32_t a : touched_) {
            mem_[a] = 0;
        }
        for (const std::uint32_t a : written_) {
            mem_[a] = 0;
        }
        touched_.clear();
        written_.clear();
    }
    std::uint8_t peek(std::uint32_t addr) const { return mem_[addr & 0xFFFFFF]; }
    const std::vector<std::uint32_t>& written() const { return written_; }

private:
    std::vector<std::uint8_t> mem_;
    std::vector<std::uint32_t> touched_;
    std::vector<std::uint32_t> written_;
};

struct Options {
    fs::path data;
    std::vector<std::string> only;
    std::size_t limit = 0;  // 0 = all
    int show = 1;
    fs::path exclusions;
    fs::path failures;
    bool injectFault = false;
    bool expectFail = false;
};

struct Exclusion {
    std::string pattern;  // exact test name, or a prefix ending in '*'
    std::string reason;
};

std::vector<Exclusion> loadExclusions(const fs::path& path) {
    std::vector<Exclusion> out;
    if (path.empty()) {
        return out;
    }
    std::ifstream in(path);
    std::string line;
    std::string sectionReason;
    bool lastWasReason = false;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '') {
            line.pop_back();
    }
    if (line.empty() || line[0] == '#') {
        continue;
    }
    if (line[0] == '@') {
        sectionReason = (lastWasReason ? sectionReason + " " : std::string()) + line.substr(1);
        lastWasReason = true;
        continue;
    }
    lastWasReason = false;
    const auto bar = line.find('|');
    Exclusion e;
    e.pattern = line.substr(0, bar);
    e.reason = bar == std::string::npos ? sectionReason : line.substr(bar + 1);
    while (!e.pattern.empty() && e.pattern.back() == ' ') {
        e.pattern.pop_back();
    }
    out.push_back(e);
}
return out;
}

bool excluded(const std::vector<Exclusion>& list, const std::string& name) {
    return std::any_of(list.begin(), list.end(), [&](const Exclusion& e) {
        if (!e.pattern.empty() && e.pattern.back() == '*') {
            return name.compare(0, e.pattern.size() - 1, e.pattern, 0, e.pattern.size() - 1) == 0;
        }
        return name == e.pattern;
    });
}

void applyState(M68k& cpu, const CpuState& in) {
    gen::m68k::State& s = cpu.state();
    for (int i = 0; i < 8; ++i) {
        s.d[i] = in.d[i];
    }
    for (int i = 0; i < 7; ++i) {
        s.a[i] = in.a[i];
    }
    s.sr = static_cast<std::uint16_t>(in.sr);
    s.a[7] = s.supervisor() ? in.ssp : in.usp;
    s.usp = in.usp;
    s.ssp = in.ssp;
    s.pc = in.pc;
    s.ir = in.prefetch[0];
    s.irc = in.prefetch[1];
    s.stopped = false;
    s.halted = false;
}

void appendHex(std::string& out, const char* label, std::uint32_t expected, std::uint32_t actual) {
    char buf[96];
    std::snprintf(buf, sizeof buf, " %s:%08X!=%08X", label, expected, actual);
    out += buf;
}

// Returns a description of every mismatching field (empty when the state matches).
std::string compare(const M68k& cpu, const FlatBus& bus, const CpuState& want) {
    const gen::m68k::State& s = cpu.state();
    std::string diff;
    static constexpr std::array<const char*, 8> kD{"d0", "d1", "d2", "d3", "d4", "d5", "d6", "d7"};
    static constexpr std::array<const char*, 7> kA{"a0", "a1", "a2", "a3", "a4", "a5", "a6"};
    for (std::size_t i = 0; i < 8; ++i) {
        if (s.d[i] != want.d[i]) {
            appendHex(diff, kD[i], want.d[i], s.d[i]);
        }
    }
    for (std::size_t i = 0; i < 7; ++i) {
        if (s.a[i] != want.a[i]) {
            appendHex(diff, kA[i], want.a[i], s.a[i]);
        }
    }
    if (s.userSp() != want.usp) {
        appendHex(diff, "usp", want.usp, s.userSp());
    }
    if (s.supervisorSp() != want.ssp) {
        appendHex(diff, "ssp", want.ssp, s.supervisorSp());
    }
    if (s.sr != want.sr) {
        appendHex(diff, "sr", want.sr, s.sr);
    }
    if (s.pc != want.pc) {
        appendHex(diff, "pc", want.pc, s.pc);
    }
    if (s.ir != want.prefetch[0] || s.irc != want.prefetch[1]) {
        appendHex(diff, "prefetch", (static_cast<std::uint32_t>(want.prefetch[0]) << 16) | want.prefetch[1],
                  (static_cast<std::uint32_t>(s.ir) << 16) | s.irc);
    }
    int ramDiffs = 0;
    for (const auto& [addr, value] : want.ram) {
        if (bus.peek(addr) != value && ramDiffs++ < 4) {
            char buf[64];
            std::snprintf(buf, sizeof buf, " ram[%06X]:%02X!=%02X", addr, value, bus.peek(addr));
            diff += buf;
        }
    }
    for (const std::uint32_t addr : bus.written()) {
        const bool known = std::any_of(want.ram.begin(), want.ram.end(),
                                       [&](const auto& e) { return (e.first & 0xFFFFFF) == addr; });
        if (!known && ramDiffs++ < 4) {
            char buf[64];
            std::snprintf(buf, sizeof buf, " unexpected-write[%06X]", addr);
            diff += buf;
        }
    }
    if (ramDiffs > 4) {
        diff += " (+" + std::to_string(ramDiffs - 4) + " ram)";
    }
    return diff;
}

std::string describeInitial(const Vector& v) {
    const CpuState& s = v.initial;
    std::ostringstream o;
    char buf[128];
    std::snprintf(buf, sizeof buf, "  initial: pc=%08X sr=%04X prefetch=%04X %04X ssp=%08X usp=%08X\n", s.pc,
                  s.sr, s.prefetch[0], s.prefetch[1], s.ssp, s.usp);
    o << buf << "          ";
    for (int i = 0; i < 8; ++i) {
        std::snprintf(buf, sizeof buf, "d%d=%08X ", i, s.d[i]);
        o << buf;
    }
    o << "\n          ";
    for (int i = 0; i < 7; ++i) {
        std::snprintf(buf, sizeof buf, "a%d=%08X ", i, s.a[i]);
        o << buf;
    }
    return o.str();
}

struct FileResult {
    std::size_t total = 0;
    std::size_t passed = 0;
    std::size_t stateFail = 0;
    std::size_t cycleOnly = 0;
    std::size_t excluded = 0;
};

FileResult runFile(const fs::path& path, const Options& opt, const std::vector<Exclusion>& exclusions,
                   std::ofstream* failures) {
    FileResult r;
    gen::test::sst::Reader reader;
    if (!reader.open(path)) {
        std::fprintf(stderr, "cannot read %s\n", path.string().c_str());
        r.stateFail = 1;
        return r;
    }
    FlatBus bus;
    M68k cpu(bus);
    cpu.setTasWriteback(true);  // the test machine has a normal read-modify-write bus
    Vector v;
    int shown = 0;
    while ((opt.limit == 0 || r.total < opt.limit) && reader.next(v)) {
        if (excluded(exclusions, v.name)) {
            ++r.excluded;
            continue;
        }
        ++r.total;
        bus.load(v.initial);
        applyState(cpu, v.initial);
        const int cycles = cpu.step();
        if (opt.injectFault && v.initial.prefetch[0] == 0x4E71) {
            cpu.state().d[0] ^= 1;  // self-test: pretend NOP corrupts D0
        }
        std::string diff = compare(cpu, bus, v.final);
        const bool cycleMismatch = static_cast<std::uint32_t>(cycles) != v.cycles;
        if (diff.empty() && !cycleMismatch) {
            ++r.passed;
        } else {
            if (diff.empty()) {
                ++r.cycleOnly;
            } else {
                ++r.stateFail;
            }
            if (cycleMismatch) {
                diff += " cycles:" + std::to_string(v.cycles) + "!=" + std::to_string(cycles);
            }
            if (failures != nullptr) {
                *failures << path.stem().string() << '\t' << v.name << '\t' << diff << '\n';
            }
            if (shown++ < opt.show) {
                std::printf("FAIL %s\n%s\n  diff (expected!=actual):%s\n", v.name.c_str(),
                            describeInitial(v).c_str(), diff.c_str());
            }
        }
        bus.clear();
    }
    return r;
}

bool parse(int argc, char** argv, Options& opt) {
    for (int i = 1; i < argc; ++i) {
        const std::string_view a = argv[i];
        const auto value = [&]() -> std::string { return i + 1 < argc ? argv[++i] : ""; };
        if (a == "--data") {
            opt.data = value();
        } else if (a == "--only") {
            std::stringstream ss(value());
            std::string item;
            while (std::getline(ss, item, ',')) {
                opt.only.push_back(item);
            }
        } else if (a == "--limit") {
            opt.limit = static_cast<std::size_t>(std::strtoul(value().c_str(), nullptr, 10));
        } else if (a == "--show") {
            opt.show = std::atoi(value().c_str());
        } else if (a == "--exclusions") {
            opt.exclusions = value();
        } else if (a == "--failures") {
            opt.failures = value();
        } else if (a == "--inject-fault") {
            opt.injectFault = true;
        } else if (a == "--expect-fail") {
            opt.expectFail = true;
        } else {
            std::fprintf(stderr, "unknown option %s\n", argv[i]);
            return false;
        }
    }
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    Options opt;
    if (!parse(argc, argv, opt)) {
        return 2;
    }
    if (opt.data.empty()) {
        if (const auto env = gen::common::getEnvPath("GENESIS_TEST_DATA")) {
            opt.data = *env / "m68k";
        } else {
            opt.data = fs::path(GENESIS_SOURCE_DIR) / "test-data" / "m68k";
        }
    }
    std::vector<fs::path> files;
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(opt.data, ec)) {
        if (entry.path().extension() != ".sst") {
            continue;
        }
        const std::string stem = entry.path().stem().string();
        if (opt.only.empty() || std::find(opt.only.begin(), opt.only.end(), stem) != opt.only.end()) {
            files.push_back(entry.path());
        }
    }
    if (files.empty()) {
        std::printf("No SingleStepTests data in %s; run: python tools/fetch-test-data.py --only m68k\n",
                    opt.data.string().c_str());
        return 4;
    }
    std::sort(files.begin(), files.end());
    const auto exclusions = loadExclusions(opt.exclusions);
    std::ofstream failureLog;
    if (!opt.failures.empty()) {
        failureLog.open(opt.failures);
    }

    FileResult total;
    std::printf("%-14s %9s %9s %9s %9s\n", "file", "pass", "total", "state", "cycles");
    for (const auto& f : files) {
        const FileResult r = runFile(f, opt, exclusions, failureLog.is_open() ? &failureLog : nullptr);
        std::printf("%-14s %9zu %9zu %9zu %9zu%s\n", f.stem().string().c_str(), r.passed, r.total,
                    r.stateFail, r.cycleOnly,
                    r.excluded != 0 ? (" (" + std::to_string(r.excluded) + " excluded)").c_str() : "");
        total.total += r.total;
        total.passed += r.passed;
        total.stateFail += r.stateFail;
        total.cycleOnly += r.cycleOnly;
        total.excluded += r.excluded;
    }
    std::printf(
        "TOTAL %zu/%zu passed (%.3f%%), %zu state mismatches, %zu cycle-only mismatches, %zu excluded\n",
        total.passed, total.total,
        total.total == 0 ? 0.0 : 100.0 * static_cast<double>(total.passed) / static_cast<double>(total.total),
        total.stateFail, total.cycleOnly, total.excluded);
    const bool allPass = total.passed == total.total;
    if (opt.expectFail) {
        return allPass ? 1 : 0;
    }
    return allPass ? 0 : 1;
}
