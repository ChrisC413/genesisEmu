#include "core/audio/psg.hpp"

#include <cmath>

namespace gen::audio {

namespace {

// 2 dB per attenuation step; 15 is off.
const std::array<int, 16> kVolume = [] {
    std::array<int, 16> t{};
    for (int i = 0; i < 15; ++i) {
        t[static_cast<std::size_t>(i)] =
            static_cast<int>(std::lround(kPsgMaxLevel * std::pow(10.0, -2.0 * i / 20.0)));
    }
    t[15] = 0;
    return t;
}();

}  // namespace

int Psg::volume(int attenuation) {
    return kVolume[static_cast<std::size_t>(attenuation & 15)];
}

void Psg::write(std::uint8_t value) {
    int reg = s_.latch;
    if ((value & 0x80) != 0) {
        reg = (value >> 4) & 7;
        s_.latch = static_cast<std::uint8_t>(reg);
    }
    const auto ch = static_cast<std::size_t>(reg >> 1);
    if ((reg & 1) != 0) {  // attenuation
        s_.attenuation[ch] = static_cast<std::uint8_t>(value & 0x0F);
        return;
    }
    if (ch == 3) {  // noise control: any write resets the shift register
        s_.noiseControl = static_cast<std::uint8_t>(value & 0x07);
        s_.lfsr = 0x8000;
        return;
    }
    if ((value & 0x80) != 0) {
        s_.period[ch] = static_cast<std::uint16_t>((s_.period[ch] & 0x3F0) | (value & 0x0F));
    } else {
        s_.period[ch] = static_cast<std::uint16_t>((s_.period[ch] & 0x00F) | ((value & 0x3F) << 4));
    }
}

void Psg::tick() {
    for (std::size_t ch = 0; ch < 3; ++ch) {
        if (s_.counter[ch] > 0) {
            --s_.counter[ch];
        }
        if (s_.counter[ch] == 0) {
            s_.counter[ch] = s_.period[ch];
            // Periods 0 and 1 hold the output high (used for sample playback through the volume register).
            s_.output[ch] = s_.period[ch] <= 1 ? std::int8_t{1} : static_cast<std::int8_t>(-s_.output[ch]);
        }
    }
    if (s_.counter[3] > 0) {
        --s_.counter[3];
    }
    if (s_.counter[3] == 0) {
        const int rate = s_.noiseControl & 3;
        s_.counter[3] = rate == 3 ? s_.period[2] : static_cast<std::uint16_t>(0x10 << rate);
        s_.noiseFlip = !s_.noiseFlip;
        if (s_.noiseFlip) {
            const bool white = (s_.noiseControl & 4) != 0;
            const unsigned in = white ? ((s_.lfsr ^ (s_.lfsr >> 3)) & 1) : (s_.lfsr & 1);
            s_.lfsr = static_cast<std::uint16_t>((s_.lfsr >> 1) | (in << 15));
            s_.output[3] = (s_.lfsr & 1) != 0 ? std::int8_t{1} : std::int8_t{-1};
        }
    }
}

int Psg::level(int ch) const {
    const auto c = static_cast<std::size_t>(ch);
    return s_.output[c] * volume(s_.attenuation[c]);
}

int Psg::mix(unsigned mask) const {
    int sum = 0;
    for (int ch = 0; ch < 4; ++ch) {
        if ((mask & (1u << ch)) != 0) {
            sum += level(ch);
        }
    }
    return sum;
}

}  // namespace gen::audio
