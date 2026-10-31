#pragma once
// Reader for the compact SingleStepTests Z80 format written by tools/fetch-test-data.py.
//
// File layout (little endian):
//   "SSTZ80\x01\x00"            8-byte magic
//   source git blob SHA-1       40 ASCII hex characters (the .json it was converted from)
//   u32 count
//   count x test:
//     u16 nameLength, name bytes (UTF-8)
//     initial state, final state:
//       u16 pc, sp, ix, iy, wz, af', bc', de', hl'
//       u8  a, f, b, c, d, e, h, l, i, r, im, iff1, iff2, ei, p, q
//       u16 ramCount, ramCount x (u16 address, u8 value)
//     u16 cycles (number of bus-state samples = T-states), u16 portCount
//     portCount x (u16 address, u8 value, u8 direction: 0 = read, 1 = write)

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

namespace gen::test::ssz {

struct CpuState {
    std::uint16_t pc = 0, sp = 0, ix = 0, iy = 0, wz = 0, af2 = 0, bc2 = 0, de2 = 0, hl2 = 0;
    std::uint8_t a = 0, f = 0, b = 0, c = 0, d = 0, e = 0, h = 0, l = 0, i = 0, r = 0, im = 0;
    std::uint8_t iff1 = 0, iff2 = 0, ei = 0, p = 0, q = 0;
    std::vector<std::pair<std::uint16_t, std::uint8_t>> ram;
};

struct Port {
    std::uint16_t addr = 0;
    std::uint8_t value = 0;
    bool write = false;
};

struct Vector {
    std::string name;
    CpuState initial;
    CpuState final;
    std::uint16_t cycles = 0;
    std::vector<Port> ports;
};

class Reader {
public:
    bool open(const std::filesystem::path& path) {
        std::ifstream in(path, std::ios::binary);
        if (!in) {
            return false;
        }
        in.seekg(0, std::ios::end);
        data_.resize(static_cast<std::size_t>(in.tellg()));
        in.seekg(0, std::ios::beg);
        in.read(reinterpret_cast<char*>(data_.data()), static_cast<std::streamsize>(data_.size()));
        static constexpr char kMagic[8] = {'S', 'S', 'T', 'Z', '8', '0', '\x01', '\x00'};
        if (data_.size() < 52 || std::memcmp(data_.data(), kMagic, 8) != 0) {
            return false;
        }
        pos_ = 48;
        count_ = static_cast<std::uint32_t>(u16() | (u16() << 16));
        return ok_;
    }

    std::uint32_t count() const { return count_; }

    bool next(Vector& v) {
        if (index_ >= count_) {
            return false;
        }
        ++index_;
        const std::uint16_t len = u16();
        if (!need(len)) {
            return false;
        }
        v.name.assign(reinterpret_cast<const char*>(data_.data() + pos_), len);
        pos_ += len;
        state(v.initial);
        state(v.final);
        v.cycles = u16();
        const std::uint16_t ports = u16();
        v.ports.resize(ports);
        for (auto& p : v.ports) {
            p.addr = u16();
            p.value = u8();
            p.write = u8() != 0;
        }
        return ok_;
    }

private:
    bool need(std::size_t n) {
        if (pos_ + n > data_.size()) {
            ok_ = false;
        }
        return ok_;
    }
    std::uint16_t u16() {
        if (!need(2)) {
            return 0;
        }
        const auto v = static_cast<std::uint16_t>(data_[pos_] | (data_[pos_ + 1] << 8));
        pos_ += 2;
        return v;
    }
    std::uint8_t u8() { return need(1) ? data_[pos_++] : 0; }
    void state(CpuState& s) {
        for (std::uint16_t* w : {&s.pc, &s.sp, &s.ix, &s.iy, &s.wz, &s.af2, &s.bc2, &s.de2, &s.hl2}) {
            *w = u16();
        }
        for (std::uint8_t* b : {&s.a, &s.f, &s.b, &s.c, &s.d, &s.e, &s.h, &s.l, &s.i, &s.r, &s.im, &s.iff1,
                                &s.iff2, &s.ei, &s.p, &s.q}) {
            *b = u8();
        }
        const std::uint16_t n = u16();
        s.ram.resize(n);
        for (auto& [addr, value] : s.ram) {
            addr = u16();
            value = u8();
        }
    }

    std::vector<std::uint8_t> data_;
    std::size_t pos_ = 0;
    std::uint32_t count_ = 0;
    std::uint32_t index_ = 0;
    bool ok_ = true;
};

}  // namespace gen::test::ssz
