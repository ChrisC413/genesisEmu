#include "core/state/state_hash.hpp"

#include <array>
#include <cstdio>

#define XXH_STATIC_LINKING_ONLY  // XXH3_state_t fields, for serialize()
#include <xxhash.h>

#include "core/state/archive.hpp"

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

void StateHasher::serialize(Archive& ar) {
    XXH3_state_t& st = *impl_->st;
    if (ar.loading()) {
        XXH3_64bits_reset(&st);  // the secret pointer refers to xxHash's built-in secret in this process
    }
    for (auto& a : st.acc) {
        std::uint64_t v = a;
        ar.u64(v);
        a = v;
    }
    ar.bytes(std::span<std::uint8_t>(st.customSecret, sizeof(st.customSecret)));
    ar.bytes(std::span<std::uint8_t>(st.buffer, sizeof(st.buffer)));
    std::uint32_t buffered = st.bufferedSize;
    std::uint32_t useSeed = st.useSeed;
    auto stripes = static_cast<std::uint64_t>(st.nbStripesSoFar);
    std::uint64_t total = st.totalLen;
    auto perBlock = static_cast<std::uint64_t>(st.nbStripesPerBlock);
    auto secretLimit = static_cast<std::uint64_t>(st.secretLimit);
    std::uint64_t seed = st.seed;
    ar.u32(buffered);
    ar.u32(useSeed);
    ar.u64(stripes);
    ar.u64(total);
    ar.u64(perBlock);
    ar.u64(secretLimit);
    ar.u64(seed);
    if (ar.loading()) {
        if (!ar.ok() || buffered > sizeof(st.buffer) || useSeed != 0 || seed != 0) {
            ar.fail("bad hash state");
            XXH3_64bits_reset(&st);
            return;
        }
        st.bufferedSize = buffered;
        st.nbStripesSoFar = static_cast<std::size_t>(stripes);
        st.totalLen = total;
        st.nbStripesPerBlock = static_cast<std::size_t>(perBlock);
        st.secretLimit = static_cast<std::size_t>(secretLimit);
    }
}

std::string hashToHex(std::uint64_t hash) {
    std::array<char, 17> buf{};
    std::snprintf(buf.data(), buf.size(), "%016llx", static_cast<unsigned long long>(hash));
    return buf.data();
}

}  // namespace gen::state
