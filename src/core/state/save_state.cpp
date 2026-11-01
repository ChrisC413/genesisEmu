#include "core/state/save_state.hpp"

#include <algorithm>

#include "core/state/archive.hpp"

namespace gen::state {

namespace {

constexpr std::uint16_t kInfoVersion = 1;
constexpr std::size_t kChunkHeaderSize = 12;

void putU16(std::vector<std::uint8_t>& out, std::uint16_t v) {
    out.push_back(static_cast<std::uint8_t>(v));
    out.push_back(static_cast<std::uint8_t>(v >> 8));
}

void putU32(std::vector<std::uint8_t>& out, std::uint32_t v) {
    for (int i = 0; i < 4; ++i) {
        out.push_back(static_cast<std::uint8_t>(v >> (8 * i)));
    }
}

std::uint32_t getU32(std::span<const std::uint8_t> b, std::size_t at) {
    return static_cast<std::uint32_t>(b[at]) | (static_cast<std::uint32_t>(b[at + 1]) << 8) |
           (static_cast<std::uint32_t>(b[at + 2]) << 16) | (static_cast<std::uint32_t>(b[at + 3]) << 24);
}

std::uint16_t getU16(std::span<const std::uint8_t> b, std::size_t at) {
    return static_cast<std::uint16_t>(b[at] | (b[at + 1] << 8));
}

bool printableTag(std::span<const std::uint8_t> tag) {
    return std::all_of(tag.begin(), tag.end(), [](std::uint8_t c) { return c >= 0x20 && c < 0x7F; });
}

}  // namespace

StateChunk* StateFile::find(std::string_view tag) {
    const auto it =
        std::find_if(chunks.begin(), chunks.end(), [&](const StateChunk& c) { return c.tag == tag; });
    return it == chunks.end() ? nullptr : &*it;
}

const StateChunk* StateFile::find(std::string_view tag) const {
    return const_cast<StateFile*>(this)->find(tag);
}

std::vector<std::uint8_t> encodeStateFile(const StateFile& file) {
    std::vector<std::uint8_t> out(kStateMagic.begin(), kStateMagic.end());
    putU32(out, file.formatVersion);
    for (const StateChunk& c : file.chunks) {
        std::string tag = c.tag;
        tag.resize(4, ' ');
        out.insert(out.end(), tag.begin(), tag.end());
        putU16(out, c.version);
        putU16(out, 0);  // flags, reserved
        putU32(out, static_cast<std::uint32_t>(c.payload.size()));
        out.insert(out.end(), c.payload.begin(), c.payload.end());
    }
    return out;
}

std::optional<StateFile> decodeStateFile(std::span<const std::uint8_t> bytes, std::string& error) {
    if (bytes.size() < kStateMagic.size() + 4 ||
        !std::equal(kStateMagic.begin(), kStateMagic.end(), bytes.begin())) {
        error = "not a genesisEmu save state";
        return std::nullopt;
    }
    StateFile file;
    file.formatVersion = getU32(bytes, kStateMagic.size());
    if (file.formatVersion == 0 || file.formatVersion > kStateFormatVersion) {
        error = "save state format version " + std::to_string(file.formatVersion) +
                " is newer than this build supports (" + std::to_string(kStateFormatVersion) + ")";
        return std::nullopt;
    }
    std::size_t pos = kStateMagic.size() + 4;
    while (pos < bytes.size()) {
        if (bytes.size() - pos < kChunkHeaderSize) {
            error = "save state is truncated (chunk header at byte " + std::to_string(pos) + ")";
            return std::nullopt;
        }
        const auto tag = bytes.subspan(pos, 4);
        if (!printableTag(tag)) {
            error = "save state is corrupt (bad chunk tag at byte " + std::to_string(pos) + ")";
            return std::nullopt;
        }
        StateChunk c;
        c.tag.assign(tag.begin(), tag.end());
        c.version = getU16(bytes, pos + 4);
        const std::uint32_t length = getU32(bytes, pos + 8);
        pos += kChunkHeaderSize;
        if (bytes.size() - pos < length) {
            error = "save state is truncated (chunk '" + c.tag + "')";
            return std::nullopt;
        }
        if (file.find(c.tag) != nullptr) {
            error = "save state is corrupt (chunk '" + c.tag + "' appears twice)";
            return std::nullopt;
        }
        c.payload.assign(bytes.begin() + static_cast<std::ptrdiff_t>(pos),
                         bytes.begin() + static_cast<std::ptrdiff_t>(pos + length));
        pos += length;
        file.chunks.push_back(std::move(c));
    }
    return file;
}

std::vector<std::uint8_t> encodeInfo(const StateInfo& info) {
    std::vector<std::uint8_t> out;
    Archive ar(out, kInfoVersion);
    StateInfo copy = info;
    ar.bytes(copy.romSha1);
    ar.u64(copy.frame);
    ar.u64(copy.timestamp);
    ar.u64(copy.stateHash);
    return out;
}

std::optional<StateInfo> decodeInfo(const StateChunk& chunk, std::string& error) {
    if (chunk.version == 0 || chunk.version > kInfoVersion) {
        error =
            "chunk 'INFO' version " + std::to_string(chunk.version) + " is newer than this build supports";
        return std::nullopt;
    }
    StateInfo info;
    Archive ar(chunk.payload, chunk.version);
    ar.bytes(info.romSha1);
    ar.u64(info.frame);
    ar.u64(info.timestamp);
    ar.u64(info.stateHash);
    if (!ar.ok() || !ar.atEnd()) {
        error = "save state is corrupt (chunk 'INFO' has the wrong size)";
        return std::nullopt;
    }
    return info;
}

std::optional<StateInfo> peekStateInfo(std::span<const std::uint8_t> bytes, std::string& error) {
    const auto file = decodeStateFile(bytes, error);
    if (!file) {
        return std::nullopt;
    }
    const StateChunk* info = file->find("INFO");
    if (info == nullptr) {
        error = "save state has no 'INFO' chunk";
        return std::nullopt;
    }
    return decodeInfo(*info, error);
}

}  // namespace gen::state
