#include "core/state/state_hash.hpp"

#include <array>
#include <cstdio>

#include <xxhash.h>

namespace gen::state {

struct StateHasher::Impl {
    XXH3_state_t* st = XXH3_createState();
    ~Impl() { XXH3_freeState(st); }
    Impl() { XXH3_64bits_reset(st); }
    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;
};

StateHasher::StateHasher() : impl_(std::make_unique<Impl>()) {}
StateHasher::~StateHasher() = default;

void StateHasher::bytes(std::span<const std::uint8_t> data) {
    XXH3_64bits_update(impl_->st, data.data(), data.size());
}

void StateHasher::u16(std::uint16_t v) {
    const std::array<std::uint8_t, 2> b{static_cast<std::uint8_t>(v), static_cast<std::uint8_t>(v >> 8)};
    bytes(b);
}

void StateHasher::u32(std::uint32_t v) {
    std::array<std::uint8_t, 4> b{};
    for (std::size_t i = 0; i < b.size(); ++i) {
        b[i] = static_cast<std::uint8_t>(v >> (8 * i));
    }
    bytes(b);
}

void StateHasher::u64(std::uint64_t v) {
    std::array<std::uint8_t, 8> b{};
    for (std::size_t i = 0; i < b.size(); ++i) {
        b[i] = static_cast<std::uint8_t>(v >> (8 * i));
    }
    bytes(b);
}

void StateHasher::tag(const char (&name)[5]) {
    bytes({reinterpret_cast<const std::uint8_t*>(name), 4});
}

std::uint64_t StateHasher::digest() const {
    return XXH3_64bits_digest(impl_->st);
}

std::string hashToHex(std::uint64_t hash) {
    std::array<char, 17> buf{};
    std::snprintf(buf.data(), buf.size(), "%016llx", static_cast<unsigned long long>(hash));
    return buf.data();
}

}  // namespace gen::state
