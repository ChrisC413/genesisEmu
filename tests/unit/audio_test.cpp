// Audio (M05): PSG tone / noise / attenuation, YM2612 through ymfm (pitch, DAC, timers), the resampler and
// the mixed output of the SoundSystem.

#include <cmath>
#include <cstdint>
#include <numbers>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "core/audio/fm_synth.hpp"
#include "core/audio/psg.hpp"
#include "core/audio/resampler.hpp"
#include "core/audio/sound.hpp"

using namespace gen::audio;

namespace {

int zeroCrossings(const std::vector<std::int32_t>& stereo, int channel) {
    int n = 0;
    for (std::size_t i = 2 + static_cast<std::size_t>(channel); i < stereo.size(); i += 2) {
        if ((stereo[i] >= 0) != (stereo[i - 2] >= 0)) {
            ++n;
        }
    }
    return n;
}

void ym(IFmSynth& fm, int part, int reg, int value) {
    fm.write(part * 2, static_cast<std::uint8_t>(reg));
    fm.write(part * 2 + 1, static_cast<std::uint8_t>(value));
}

}  // namespace

TEST_CASE("PSG tone period counts ticks", "[audio][psg]") {
    Psg psg;
    psg.write(0x80 | 0x00);  // tone 1 period low nibble 0
    psg.write(0x10);         // period = 0x100
    psg.write(0x90);         // tone 1 attenuation 0
    int flips = 0;
    int last = psg.level(0);
    for (int i = 0; i < 0x100 * 10; ++i) {
        psg.tick();
        if (psg.level(0) != last) {
            ++flips;
            last = psg.level(0);
        }
    }
    CHECK(flips == 10);  // one half-period per 0x100 ticks
    CHECK(std::abs(last) == kPsgMaxLevel);
}

TEST_CASE("PSG attenuation is 2 dB per step, 15 is silent", "[audio][psg]") {
    for (int i = 0; i < 14; ++i) {
        CHECK(static_cast<double>(Psg::volume(i + 1)) / Psg::volume(i) ==
              Catch::Approx(0.7943).epsilon(0.01));
    }
    CHECK(Psg::volume(15) == 0);
}

TEST_CASE("PSG noise: periodic period 16, white noise LFSR period 57337 (Sega taps)", "[audio][psg]") {
    Psg psg;
    psg.write(0xE0);  // periodic noise, rate /16 (resets the LFSR)
    auto shifts = [&](int n) {
        for (int i = 0; i < n; ++i) {
            const std::uint16_t before = psg.state().lfsr;
            while (psg.state().lfsr == before) {
                psg.tick();
            }
        }
    };
    const std::uint16_t start = psg.state().lfsr;
    shifts(16);
    CHECK(psg.state().lfsr == start);
    psg.write(0xE4);  // white noise
    shifts(100);
    const std::uint16_t mark = psg.state().lfsr;
    int period = 0;
    do {
        shifts(1);
        ++period;
    } while (psg.state().lfsr != mark && period < 70000);
    CHECK(period == 57337);
}

TEST_CASE("YM2612 sine voice plays at the programmed pitch", "[audio][fm]") {
    auto fm = makeYmfmSynth();
    ym(*fm, 0, 0xB0, 0x07);  // algorithm 7 (all carriers), no feedback
    ym(*fm, 0, 0xB4, 0xC0);  // left + right
    for (int slot = 0; slot < 4; ++slot) {
        ym(*fm, 0, 0x30 + slot * 4, 0x01);                     // MUL 1
        ym(*fm, 0, 0x40 + slot * 4, slot == 3 ? 0x00 : 0x7F);  // only operator 4 audible
        ym(*fm, 0, 0x50 + slot * 4, 0x1F);                     // fastest attack
        ym(*fm, 0, 0x60 + slot * 4, 0x00);
        ym(*fm, 0, 0x70 + slot * 4, 0x00);
        ym(*fm, 0, 0x80 + slot * 4, 0x0F);
    }
    // 440 Hz at block 4: fnum = 440 * 2^20 / (53267 * 2^3) = 1083.
    ym(*fm, 0, 0xA4, (4 << 3) | (1083 >> 8));
    ym(*fm, 0, 0xA0, 1083 & 0xFF);
    ym(*fm, 0, 0x28, 0xF0);  // key on, channel 1
    std::vector<std::int32_t> out(2 * 53267);
    fm->generate(out.data(), 53267, 0x7F);
    CHECK(zeroCrossings(out, 0) / 2 == Catch::Approx(440).epsilon(0.01));
    std::int32_t peak = 0;
    for (const auto v : out) {
        peak = std::max(peak, std::abs(v));
    }
    CHECK(peak > 1000);
    // Muting channel 1 silences it (only the DAC-discontinuity offset remains, identical on every sample).
    fm->generate(out.data(), 1000, 0x7E);
    CHECK(out[0] == out[1998]);

    // FM state round-trips (save states): restoring replays the same samples.
    const std::vector<std::uint8_t> saved = fm->serialize();
    std::vector<std::int32_t> first(2 * 500);
    fm->generate(first.data(), 500, 0x7F);
    fm->deserialize(saved);
    std::vector<std::int32_t> second(2 * 500);
    fm->generate(second.data(), 500, 0x7F);
    CHECK(first == second);
}

TEST_CASE("YM2612 DAC replaces channel 6", "[audio][fm]") {
    auto fm = makeYmfmSynth();
    ym(*fm, 1, 0xB6, 0xC0);  // channel 6 to both sides
    ym(*fm, 0, 0x2B, 0x80);  // DAC on
    ym(*fm, 0, 0x2A, 0xFF);
    std::vector<std::int32_t> hi(20);
    fm->generate(hi.data(), 10, 0x7F);
    ym(*fm, 0, 0x2A, 0x00);
    std::vector<std::int32_t> lo(20);
    fm->generate(lo.data(), 10, 0x7F);
    CHECK(hi[18] > lo[18] + 1000);
    std::vector<std::int32_t> muted(20);
    fm->generate(muted.data(), 10, 0x3F);  // DAC muted
    CHECK(std::abs(muted[18]) < std::abs(lo[18]));
    std::vector<std::int32_t> fm6Muted(20);
    fm->generate(fm6Muted.data(), 10, 0x5F);  // FM 6 muted: the DAC replaces channel 6, so it is silent too
    CHECK(fm6Muted[18] == muted[18]);
}

TEST_CASE("YM2612 timer A sets its status flag", "[audio][fm]") {
    auto fm = makeYmfmSynth();
    ym(*fm, 0, 0x24, 0xFF);  // timer A = 1023: the shortest period
    ym(*fm, 0, 0x25, 0x03);
    ym(*fm, 0, 0x27, 0x05);  // load A, flag enable A
    CHECK((fm->readStatus() & 0x01) == 0);
    std::vector<std::int32_t> out(2 * 10);
    fm->generate(out.data(), 10, 0x7F);
    CHECK((fm->readStatus() & 0x01) != 0);
    ym(*fm, 0, 0x27, 0x15);  // reset flag A
    CHECK((fm->readStatus() & 0x01) == 0);
}

TEST_CASE("Resampler keeps pitch and level", "[audio][resampler]") {
    const double inRate = kMclkNtsc / kMclkPerSample;
    Resampler rs(inRate, 48000);
    std::vector<std::int16_t> in;
    for (int i = 0; i < 53267; ++i) {
        const auto v = static_cast<std::int16_t>(
            std::lround(10000 * std::sin(2 * std::numbers::pi * 1000 * i / inRate)));
        in.push_back(v);
        in.push_back(v);
    }
    std::vector<std::int16_t> out;
    rs.process(in, out);
    const std::size_t frames = out.size() / 2;
    CHECK(static_cast<double>(frames) == Catch::Approx(48000).margin(40));
    int crossings = 0;
    int peak = 0;
    for (std::size_t i = 1000; i < frames; ++i) {
        if ((out[2 * i] >= 0) != (out[2 * i - 2] >= 0)) {
            ++crossings;
        }
        peak = std::max(peak, std::abs(static_cast<int>(out[2 * i])));
    }
    CHECK(crossings / 2.0 / ((frames - 1000) / 48000.0) == Catch::Approx(1000).epsilon(0.005));
    CHECK(peak == Catch::Approx(10000).epsilon(0.02));
    rs.setRatioAdjust(1.05);
    CHECK(rs.ratioAdjust() == Catch::Approx(1.005));  // dynamic rate control is limited to +-0.5%
}

TEST_CASE("SoundSystem renders one native sample per 1008 master clocks", "[audio]") {
    SoundSystem s;
    s.reset(false);
    s.advanceTo(1008 * 100 + 500);
    CHECK(s.samples().size() == 200);
    const auto h1 = s.audioHash();
    s.advanceTo(1008 * 100 + 900);  // still inside the same sample
    CHECK(s.samples().size() == 200);
    CHECK(s.audioHash() == h1);
    CHECK(s.sampleRate() == Catch::Approx(53267.0).margin(1));
}
