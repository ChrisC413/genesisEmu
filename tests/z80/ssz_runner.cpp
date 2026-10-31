// SingleStepTests Z80 runner (story M04-04).
//
//   gen_z80_sst [--data DIR] [--only PREFIX,...] [--limit N] [--show N] [--inject-fault] [--expect-fail]
//
// Streams each converted .ssz file (tools/fetch-test-data.py), runs one instruction per vector against a flat
// 64 KB bus and compares every register (including WZ, R, Q, the EI/LD A,I latches), RAM, port writes and
// the T-state count. Prints one line per opcode file that has failures, plus a total. Exit codes: 0 all
// pass, 1 failures, 2 usage error, 4 no test data (CTest reports "skipped").

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "core/z80/z80.hpp"
#include "ssz_format.hpp"

namespace fs = std::filesystem;
using gen::test::ssz::CpuState;
using gen::test::ssz::Vector;

namespace {

class FlatBus final : public gen::z80::BusZ80 {
public:
    std::array<std::uint8_t, 0x10000> mem{};
    std::vector<gen::test::ssz::Port> portsIn;
    std::vector<gen::test::ssz::Port> portsOut;
    std::uint8_t read8(std::uint16_t addr) override { return mem[addr]; }
    void write8(std::uint16_t addr, std::uint8_t v) override { mem[addr] = v; }
    std::uint8_t in8(std::uint16_t port) override {
        for (const auto& p : portsIn) {
            if (!p.write && p.addr == port) {
                return p.value;
            }
        }
        return 0xFF;
    }
    void out8(std::uint16_t port, std::uint8_t v) override { portsOut.push_back({port, v, true}); }
};

struct Options {
    fs::path data;
    std::vector<std::string> only;
    std::size_t limit = 0;
    int show = 5;
    bool injectFault = false;
    bool expectFail = false;
};

void load(gen::z80::Z80State& s, const CpuState& t) {
    s = gen::z80::Z80State{};
    s.pc = t.pc;
    s.sp = t.sp;
    s.ix = t.ix;
    s.iy = t.iy;
    s.wz = t.wz;
    s.af2 = t.af2;
    s.bc2 = t.bc2;
    s.de2 = t.de2;
    s.hl2 = t.hl2;
    s.a = t.a;
    s.f = t.f;
    s.b = t.b;
    s.c = t.c;
    s.d = t.d;
    s.e = t.e;
    s.h = t.h;
    s.l = t.l;
    s.i = t.i;
    s.r = t.r;
    s.im = t.im;
    s.iff1 = t.iff1 != 0;
    s.iff2 = t.iff2 != 0;
    s.eiDelay = t.ei != 0;
    s.ldAir = t.p != 0;
    s.q = t.q;
}

std::string compare(const gen::z80::Z80State& s, const Vector& v, const FlatBus& bus, int cycles) {
    std::string diff;
    const CpuState& t = v.final;
    const auto chk = [&](const char* name, unsigned expected, unsigned actual) {
        if (expected != actual) {
            char buf[64];
            std::snprintf(buf, sizeof buf, " %s:%X!=%X", name, expected, actual);
            diff += buf;
        }
    };
    chk("pc", t.pc, s.pc);
    chk("sp", t.sp, s.sp);
    chk("ix", t.ix, s.ix);
    chk("iy", t.iy, s.iy);
    chk("wz", t.wz, s.wz);
    chk("af'", t.af2, s.af2);
    chk("bc'", t.bc2, s.bc2);
    chk("de'", t.de2, s.de2);
    chk("hl'", t.hl2, s.hl2);
    chk("a", t.a, s.a);
    chk("f", t.f, s.f);
    chk("b", t.b, s.b);
    chk("c", t.c, s.c);
    chk("d", t.d, s.d);
    chk("e", t.e, s.e);
    chk("h", t.h, s.h);
    chk("l", t.l, s.l);
    chk("i", t.i, s.i);
    chk("r", t.r, s.r);
    chk("im", t.im, s.im);
    chk("iff1", t.iff1, s.iff1 ? 1U : 0U);
    chk("iff2", t.iff2, s.iff2 ? 1U : 0U);
    chk("ei", t.ei, s.eiDelay ? 1U : 0U);
    chk("p", t.p, s.ldAir ? 1U : 0U);
    chk("q", t.q, s.q);
    chk("cycles", v.cycles, static_cast<unsigned>(cycles));
    for (const auto& [addr, value] : t.ram) {
        if (bus.mem[addr] != value) {
            char buf[48];
            std::snprintf(buf, sizeof buf, " ram[%04X]:%02X!=%02X", addr, value, bus.mem[addr]);
            diff += buf;
        }
    }
    std::vector<gen::test::ssz::Port> expectedOut;
    for (const auto& p : v.ports) {
        if (p.write) {
            expectedOut.push_back(p);
        }
    }
    bool portsOk = expectedOut.size() == bus.portsOut.size();
    for (std::size_t i = 0; portsOk && i < expectedOut.size(); ++i) {
        portsOk = expectedOut[i].addr == bus.portsOut[i].addr && expectedOut[i].value == bus.portsOut[i].value;
    }
    if (!portsOk) {
        diff += " port-writes";
    }
    return diff;
}

struct FileResult {
    std::size_t passed = 0;
    std::size_t total = 0;
};

FileResult runFile(const fs::path& path, const Options& opt, int& shown) {
    FileResult r;
    gen::test::ssz::Reader reader;
    if (!reader.open(path)) {
        std::fprintf(stderr, "cannot read %s\n", path.string().c_str());
        return r;
    }
    FlatBus bus;
    gen::z80::Z80 cpu(bus);
    Vector v;
    while (reader.next(v) && (opt.limit == 0 || r.total < opt.limit)) {
        ++r.total;
        bus.mem.fill(0);
        for (const auto& [addr, value] : v.initial.ram) {
            bus.mem[addr] = value;
        }
        bus.portsIn = v.ports;
        bus.portsOut.clear();
        load(cpu.state(), v.initial);
        const int cycles = cpu.step();
        if (opt.injectFault) {
            cpu.state().a ^= 0x01;
        }
        const std::string diff = compare(cpu.state(), v, bus, cycles);
        if (diff.empty()) {
            ++r.passed;
        } else if (shown < opt.show) {
            ++shown;
            std::printf("FAIL %s (expected!=actual):%s\n", v.name.c_str(), diff.c_str());
        }
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
            std::string list = value();
            std::size_t start = 0;
            while (start <= list.size()) {
                const std::size_t comma = list.find(',', start);
                opt.only.push_back(list.substr(start, comma - start));
                if (comma == std::string::npos) {
                    break;
                }
                start = comma + 1;
            }
        } else if (a == "--limit") {
            opt.limit = std::stoul(value());
        } else if (a == "--show") {
            opt.show = std::stoi(value());
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
        opt.data = fs::path(GENESIS_SOURCE_DIR) / "test-data" / "z80";
    }
    std::vector<fs::path> files;
    std::error_code ec;
    for (const auto& e : fs::directory_iterator(opt.data, ec)) {
        if (e.path().extension() != ".ssz") {
            continue;
        }
        const std::string stem = e.path().stem().string();
        if (!opt.only.empty() && std::none_of(opt.only.begin(), opt.only.end(),
                                              [&](const std::string& o) { return stem.starts_with(o); })) {
            continue;
        }
        files.push_back(e.path());
    }
    if (files.empty()) {
        std::printf("No SingleStepTests Z80 data in %s; run: python tools/fetch-test-data.py --only z80\n",
                    opt.data.string().c_str());
        return 4;
    }
    std::sort(files.begin(), files.end());
    std::size_t passed = 0;
    std::size_t total = 0;
    std::size_t badFiles = 0;
    int shown = 0;
    for (const auto& f : files) {
        const FileResult r = runFile(f, opt, shown);
        passed += r.passed;
        total += r.total;
        if (r.passed != r.total) {
            ++badFiles;
            std::printf("%-14s %6zu / %6zu\n", f.stem().string().c_str(), r.passed, r.total);
        }
    }
    std::printf("z80 SingleStepTests: %zu / %zu passed (%.4f%%) in %zu files, %zu with failures\n", passed, total,
                total != 0 ? 100.0 * static_cast<double>(passed) / static_cast<double>(total) : 0.0,
                files.size(), badFiles);
    const bool allPass = passed == total;
    if (opt.expectFail) {
        return allPass ? 1 : 0;
    }
    return allPass ? 0 : 1;
}
