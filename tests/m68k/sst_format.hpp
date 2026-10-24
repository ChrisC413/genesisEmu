#pragma once
// Reader for the compact SingleStepTests format written by tools/fetch-test-data.py.
//
// File layout (little endian):
//   "SST68K\x01\x00"            8-byte magic
//   source git blob SHA-1       40 ASCII hex characters (the .json.gz it was converted from)
//   u32 count
//   count x test:
//     u16 nameLength, name bytes (UTF-8)
//     initial state, final state:
//       u32 d0-d7, a0-a6, usp, ssp, sr, pc   (19 x u32; pc = address of the opcode in prefetch[0])
//       u16 prefetch[2]                       (IR, IRC)
//       u32 ramCount, ramCount x (u32 address, u8 value)
//     u32 cycles

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

namespace gen::test::sst {

struct CpuState {
    std::uint32_t d[8]{};
    std::uint32_t a[7]{};
    std::uint32_t usp = 0;
    std::uint32_t ssp = 0;
    std::uint32_t sr = 0;
    std::uint32_t pc = 0;
    std::uint16_t prefetch[2]{};
    std::vector<std::pair<std::uint32_t, std::uint8_t>> ram;
};

struct Vector {
    std::string name;
    CpuState initial;
    CpuState final;
    std::uint32_t cycles = 0;
};

class Reader {
public:
    bool open(const std::filesystem::path& path) {
        std::ifstream in(path, std::ios::binary);
        if (!in) {
            return false;
        }
        data_.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
        static constexpr char kMagic[8] = {'S', 'S', 'T', '6', '8', 'K', '\x01', '\x00'};
        if (data_.size() < 52 || std::memcmp(data_.data(), kMagic, 8) != 0) {
            return false;
        }
        pos_ = 48;
        count_ = u32();
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
        v.name.assign(data_.data() + pos_, len);
        pos_ += len;
        state(v.initial);
        state(v.final);
        v.cycles = u32();
        return ok_;
    }

private:
    bool need(std::size_t n) {
        if (pos_ + n > data_.size()) {
            ok_ = false;
        }
        return ok_;
    }
    std::uint32_t u32() {
        if (!need(4)) {
            return 0;
        }
        std::uint32_t v = 0;
        for (int i = 3; i >= 0; --i) {
            v = (v << 8) | static_cast<std::uint8_t>(data_[pos_ + static_cast<std::size_t>(i)]);
        }
        pos_ += 4;
        return v;
    }
    std::uint16_t u16() {
        if (!need(2)) {
            return 0;
        }
        const auto v = static_cast<std::uint16_t>(static_cast<std::uint8_t>(data_[pos_]) |
                                                  (static_cast<std::uint8_t>(data_[pos_ + 1]) << 8));
        pos_ += 2;
        return v;
    }
    std::uint8_t u8() {
        if (!need(1)) {
            return 0;
        }
        return static_cast<std::uint8_t>(data_[pos_++]);
    }
    void state(CpuState& s) {
        for (auto& r : s.d) {
            r = u32();
        }
        for (auto& r : s.a) {
            r = u32();
        }
        s.usp = u32();
        s.ssp = u32();
        s.sr = u32();
        s.pc = u32();
        s.prefetch[0] = u16();
        s.prefetch[1] = u16();
        const std::uint32_t n = u32();
        s.ram.clear();
        if (!need(static_cast<std::size_t>(n) * 5)) {
            return;
        }
        s.ram.reserve(n);
        for (std::uint32_t i = 0; i < n; ++i) {
            const std::uint32_t addr = u32();
            s.ram.emplace_back(addr, u8());
        }
    }

    std::vector<char> data_;
    std::size_t pos_ = 0;
    std::uint32_t count_ = 0;
    std::uint32_t index_ = 0;
    bool ok_ = true;
};

}  // namespace gen::test::sst
