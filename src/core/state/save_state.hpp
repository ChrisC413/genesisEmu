#pragma once
// Save-state container (spec 08 "Save states", M06-00a). A state file is
//
//   "GENSTATE"            8-byte magic
//   u32 format version    kStateFormatVersion; a newer container is refused
//   chunks to the end:    tag (4 ASCII bytes), u16 chunk version, u16 flags (0), u32 length, payload
//
// all little-endian. The "INFO" chunk (StateInfo) names the ROM by SHA-1, the frame, a timestamp and the
// machine's state hash at save time; the other chunks hold one component each (Machine::saveState lists
// them). This header only frames chunks; Machine::loadState interprets them.

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "core/util/hash.hpp"

namespace gen::state {

inline constexpr std::string_view kStateMagic = "GENSTATE";
inline constexpr std::uint32_t kStateFormatVersion = 1;
// Conventional file extension of a state file.
inline constexpr std::string_view kStateExtension = ".gst";

struct StateChunk {
    std::string tag;  // exactly 4 characters
    std::uint16_t version = 1;
    std::vector<std::uint8_t> payload;
};

struct StateFile {
    std::uint32_t formatVersion = kStateFormatVersion;
    std::vector<StateChunk> chunks;

    StateChunk* find(std::string_view tag);
    const StateChunk* find(std::string_view tag) const;
};

// Contents of the "INFO" chunk.
struct StateInfo {
    Sha1Digest romSha1{};
    std::uint64_t frame = 0;
    std::uint64_t timestamp = 0;  // seconds since the Unix epoch; 0 = not recorded
    std::uint64_t stateHash = 0;  // Machine::stateHash() when saved; loading verifies it
};

std::vector<std::uint8_t> encodeStateFile(const StateFile& file);
// Checks the magic, the container version and the chunk framing; payloads are not interpreted.
std::optional<StateFile> decodeStateFile(std::span<const std::uint8_t> bytes, std::string& error);

std::vector<std::uint8_t> encodeInfo(const StateInfo& info);
std::optional<StateInfo> decodeInfo(const StateChunk& chunk, std::string& error);
// Reads just the INFO chunk of a state file (slot menus, ROM checks before loading).
std::optional<StateInfo> peekStateInfo(std::span<const std::uint8_t> bytes, std::string& error);

}  // namespace gen::state
