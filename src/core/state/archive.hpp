#pragma once
// Field-by-field (de)serialization of one save-state chunk (spec 01 "Save-state serialization", M06-00a).
// The same function saves and loads a component: in saving mode every call appends the field, in loading
// mode it overwrites the field from the payload. Values are little-endian; struct layout never reaches the
// file. A read past the end of the payload, or a size that does not match, marks the archive failed and
// leaves the remaining fields untouched.

#include <array>
#include <bit>
#include <cstdint>
#include <cstring>
#include <span>
#include <string>
#include <vector>

namespace gen::state {

class Archive {
public:
    // Saving: appends to `out`.
    Archive(std::vector<std::uint8_t>& out, std::uint16_t version) : out_(&out), version_(version) {}
    // Loading: reads `in`, a chunk payload written with `version`.
    Archive(std::span<const std::uint8_t> in, std::uint16_t version) : in_(in), version_(version) {}

    bool loading() const { return out_ == nullptr; }
    // Version of the chunk being read or written; serializers branch on it to read older layouts.
    std::uint16_t version() const { return version_; }
    bool ok() const { return error_.empty(); }
    const std::string& error() const { return error_; }
    // Loading: the whole payload was consumed.
    bool atEnd() const { return pos_ == in_.size(); }
    void fail(std::string why) {
        if (error_.empty()) {
            error_ = std::move(why);
        }
    }

    void bytes(std::span<std::uint8_t> data) {
        if (!loading()) {
            out_->insert(out_->end(), data.begin(), data.end());
            return;
        }
        if (!ok() || in_.size() - pos_ < data.size()) {
            fail("truncated");
            return;
        }
        std::memcpy(data.data(), in_.data() + pos_, data.size());
        pos_ += data.size();
    }
    template <std::size_t N>
    void bytes(std::array<std::uint8_t, N>& a) {
        bytes(std::span<std::uint8_t>(a));
    }

    void u8(std::uint8_t& v) { bytes(std::span<std::uint8_t>(&v, 1)); }
    void i8(std::int8_t& v) {
        auto u = static_cast<std::uint8_t>(v);
        u8(u);
        v = static_cast<std::int8_t>(u);
    }
    void boolean(bool& v) {
        std::uint8_t b = v ? 1 : 0;
        u8(b);
        v = b != 0;
    }
    void u16(std::uint16_t& v) { little(v); }
    void u32(std::uint32_t& v) { little(v); }
    void u64(std::uint64_t& v) { little(v); }
    void i32(int& v) {
        static_assert(sizeof(int) == 4);
        auto u = static_cast<std::uint32_t>(v);
        u32(u);
        v = static_cast<int>(u);
    }
    void f32(float& v) {
        auto u = std::bit_cast<std::uint32_t>(v);
        u32(u);
        v = std::bit_cast<float>(u);
    }
    template <class E>
    void enum8(E& e) {
        auto u = static_cast<std::uint8_t>(e);
        u8(u);
        e = static_cast<E>(u);
    }
    template <std::size_t N>
    void u16s(std::array<std::uint16_t, N>& a) {
        for (auto& v : a) {
            u16(v);
        }
    }
    // A variable-length byte block (u32 length + bytes).
    void blob(std::vector<std::uint8_t>& v) {
        auto n = static_cast<std::uint32_t>(v.size());
        u32(n);
        if (loading()) {
            if (!ok() || in_.size() - pos_ < n) {
                fail("truncated");
                return;
            }
            v.assign(in_.begin() + static_cast<std::ptrdiff_t>(pos_),
                     in_.begin() + static_cast<std::ptrdiff_t>(pos_ + n));
            pos_ += n;
            return;
        }
        out_->insert(out_->end(), v.begin(), v.end());
    }
    // A byte block whose size is fixed by the running machine (e.g. SRAM): loading fails on a mismatch.
    void fixedBlob(std::span<std::uint8_t> data) {
        auto n = static_cast<std::uint32_t>(data.size());
        u32(n);
        if (loading() && ok() && n != data.size()) {
            fail("size mismatch");
            return;
        }
        bytes(data);
    }
    void string(std::string& s) {
        std::vector<std::uint8_t> v(s.begin(), s.end());
        blob(v);
        s.assign(v.begin(), v.end());
    }

private:
    template <class T>
    void little(T& v) {
        std::array<std::uint8_t, sizeof(T)> b{};
        for (std::size_t i = 0; i < sizeof(T); ++i) {
            b[i] = static_cast<std::uint8_t>(v >> (8 * i));
        }
        bytes(b);
        if (loading() && ok()) {
            T r = 0;
            for (std::size_t i = 0; i < sizeof(T); ++i) {
                r = static_cast<T>(r | (static_cast<T>(b[i]) << (8 * i)));
            }
            v = r;
        }
    }

    std::vector<std::uint8_t>* out_ = nullptr;
    std::span<const std::uint8_t> in_;
    std::size_t pos_ = 0;
    std::uint16_t version_ = 1;
    std::string error_;
};

}  // namespace gen::state
