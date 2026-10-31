#pragma once
// WAV recording of the mixed audio output (spec 06 A-4, M05-06): 16-bit PCM, streamed to disk.

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <span>

namespace gen::common {

class WavWriter {
public:
    WavWriter() = default;
    ~WavWriter() { close(); }
    WavWriter(const WavWriter&) = delete;
    WavWriter& operator=(const WavWriter&) = delete;

    // Starts a file; `rate` is rounded to whole Hz. Returns false if it cannot be created.
    bool open(const std::filesystem::path& path, double rate, int channels = 2);
    // Interleaved samples.
    void write(std::span<const std::int16_t> samples);
    // Patches the header sizes and closes the file.
    void close();

    bool isOpen() const { return out_.is_open(); }
    std::uint64_t frames() const {
        return channels_ != 0 ? bytes_ / (2u * static_cast<unsigned>(channels_)) : 0;
    }

private:
    std::ofstream out_;
    std::uint64_t bytes_ = 0;
    int channels_ = 2;
};

}  // namespace gen::common
