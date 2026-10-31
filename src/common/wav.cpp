#include "common/wav.hpp"

#include <array>
#include <cmath>

namespace gen::common {

namespace {

void put32(std::ofstream& out, std::uint32_t v) {
    const std::array<char, 4> b{static_cast<char>(v), static_cast<char>(v >> 8), static_cast<char>(v >> 16),
                                static_cast<char>(v >> 24)};
    out.write(b.data(), 4);
}

void put16(std::ofstream& out, std::uint16_t v) {
    const std::array<char, 2> b{static_cast<char>(v), static_cast<char>(v >> 8)};
    out.write(b.data(), 2);
}

}  // namespace

bool WavWriter::open(const std::filesystem::path& path, double rate, int channels) {
    close();
    out_.open(path, std::ios::binary | std::ios::trunc);
    if (!out_) {
        return false;
    }
    channels_ = channels;
    bytes_ = 0;
    const auto hz = static_cast<std::uint32_t>(std::lround(rate));
    out_.write("RIFF", 4);
    put32(out_, 36);  // patched in close()
    out_.write("WAVEfmt ", 8);
    put32(out_, 16);
    put16(out_, 1);  // PCM
    put16(out_, static_cast<std::uint16_t>(channels));
    put32(out_, hz);
    put32(out_, hz * static_cast<std::uint32_t>(channels) * 2);
    put16(out_, static_cast<std::uint16_t>(channels * 2));
    put16(out_, 16);
    out_.write("data", 4);
    put32(out_, 0);
    return static_cast<bool>(out_);
}

void WavWriter::write(std::span<const std::int16_t> samples) {
    if (!out_.is_open()) {
        return;
    }
    for (const std::int16_t s : samples) {
        put16(out_, static_cast<std::uint16_t>(s));
    }
    bytes_ += samples.size() * 2;
}

void WavWriter::close() {
    if (!out_.is_open()) {
        return;
    }
    const auto data = static_cast<std::uint32_t>(bytes_);
    out_.seekp(4);
    put32(out_, 36 + data);
    out_.seekp(40);
    put32(out_, data);
    out_.close();
}

}  // namespace gen::common
