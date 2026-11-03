#pragma once
// Streaming 64-bit state hash (XXH3) over explicitly serialized fields. Values are fed as little-endian
// bytes field by field, so struct padding and host byte order never reach the hash (D-011).

#include <cstdint>
#include <memory>
#include <span>
#include <string>

namespace gen::state {

class Archive;

class StateHasher {
public:
    StateHasher();
    ~StateHasher();
    StateHasher(const StateHasher&) = delete;
    StateHasher& operator=(const StateHasher&) = delete;

    void bytes(std::span<const std::uint8_t> data);
    void u8(std::uint8_t v) { bytes({&v, 1}); }
    void boolean(bool v) { u8(v ? 1 : 0); }
    void u16(std::uint16_t v);
    void u32(std::uint32_t v);
    void u64(std::uint64_t v);
    void i32(std::int32_t v) { u32(static_cast<std::uint32_t>(v)); }
    // A section tag keeps fields of different components from aliasing.
    void tag(const char (&name)[5]);

    std::uint64_t digest() const;

    // Saves / restores the running hash (the audio hash continues across a save state). The xxHash state is
    // written field by field, so the bytes do not depend on padding or host byte order.
    void serialize(Archive& ar);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

std::string hashToHex(std::uint64_t hash);

}  // namespace gen::state
