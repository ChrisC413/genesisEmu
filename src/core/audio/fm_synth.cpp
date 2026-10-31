#include "core/audio/fm_synth.hpp"

#include <array>

#include "ymfm_opn.h"

namespace gen::audio {

namespace {

// ymfm's host interface: timers and the busy flag in YM input clocks, advanced as samples are generated.
class Interface final : public ymfm::ymfm_interface {
public:
    void ymfm_set_timer(uint32_t tnum, int32_t durationInClocks) override {
        timers_[tnum & 1] = durationInClocks;
    }
    void ymfm_set_busy_end(uint32_t clocks) override { busyEnd_ = now_ + clocks; }
    bool ymfm_is_busy() override { return now_ < busyEnd_; }

    // Advances one output sample (144 clocks) and fires expired timers.
    void advance() {
        now_ += kFmSampleClocks;
        for (std::uint32_t t = 0; t < 2; ++t) {
            if (timers_[t] < 0) {
                continue;
            }
            timers_[t] -= kFmSampleClocks;
            if (timers_[t] <= 0) {
                const std::int32_t overshoot = timers_[t];
                timers_[t] = -1;
                m_engine->engine_timer_expired(t);  // reloads the timer through ymfm_set_timer
                if (timers_[t] > 0) {
                    timers_[t] += overshoot;
                }
            }
        }
    }
    void reset() {
        timers_ = {-1, -1};
        busyEnd_ = now_;
    }
    // ymfm's saved-state helper has no 64-bit overloads: split into 32-bit halves.
    void saveRestore(ymfm::ymfm_saved_state& st) {
        auto nowLo = static_cast<std::uint32_t>(now_);
        auto nowHi = static_cast<std::uint32_t>(now_ >> 32);
        auto busyLo = static_cast<std::uint32_t>(busyEnd_);
        auto busyHi = static_cast<std::uint32_t>(busyEnd_ >> 32);
        st.save_restore(nowLo);
        st.save_restore(nowHi);
        st.save_restore(busyLo);
        st.save_restore(busyHi);
        st.save_restore(timers_[0]);
        st.save_restore(timers_[1]);
        now_ = (static_cast<std::uint64_t>(nowHi) << 32) | nowLo;
        busyEnd_ = (static_cast<std::uint64_t>(busyHi) << 32) | busyLo;
    }

private:
    std::uint64_t now_ = 0;
    std::uint64_t busyEnd_ = 0;
    std::array<std::int32_t, 2> timers_{-1, -1};
};

// ymfm's YM2612 with per-channel muting: the same output stage as ymfm::ym2612::generate (DAC discontinuity,
// DAC replacing channel 6), restricted to the unmuted channels.
class MaskedYm2612 final : public ymfm::ym2612 {
public:
    explicit MaskedYm2612(ymfm::ymfm_interface& intf) : ymfm::ym2612(intf) {}

    void render(std::int32_t* stereo, int frames, unsigned mask, Interface& intf) {
        for (int i = 0; i < frames; ++i) {
            m_fm.clock(fm_engine::ALL_CHANNELS);
            intf.advance();
            std::int32_t left = 0;
            std::int32_t right = 0;
            output_data temp;
            const int lastFm = m_dac_enable != 0 ? 5 : 6;
            for (int ch = 0; ch < lastFm; ++ch) {
                if ((mask & (1u << ch)) == 0) {
                    left += dac_discontinuity(0);
                    right += dac_discontinuity(0);
                    continue;
                }
                temp.clear();
                m_fm.output(temp, 5, 256, 1u << ch);
                left += dac_discontinuity(temp.data[0]);
                right += dac_discontinuity(temp.data[1]);
            }
            if (m_dac_enable != 0) {
                const bool on = (mask & (1u << kFmChannelDac)) != 0;
                const std::int32_t dac = dac_discontinuity(on ? (static_cast<std::int16_t>(m_dac_data << 7) >> 7) : 0);
                left += m_fm.regs().ch_output_0(0x102) != 0 ? dac : dac_discontinuity(0);
                right += m_fm.regs().ch_output_1(0x102) != 0 ? dac : dac_discontinuity(0);
            }
            stereo[2 * i] = left * 128 * 64 / (6 * 65);
            stereo[2 * i + 1] = right * 128 * 64 / (6 * 65);
        }
    }
};

class YmfmSynth final : public IFmSynth {
public:
    YmfmSynth() : chip_(intf_) { chip_.reset(); }

    void reset() override {
        intf_.reset();
        chip_.reset();
    }
    void write(int port, std::uint8_t value) override { chip_.write(static_cast<std::uint32_t>(port & 3), value); }
    std::uint8_t readStatus() override { return chip_.read_status(); }
    void generate(std::int32_t* stereo, int frames, unsigned channelMask) override {
        chip_.render(stereo, frames, channelMask, intf_);
    }
    std::vector<std::uint8_t> serialize() override {
        std::vector<std::uint8_t> buf;
        ymfm::ymfm_saved_state st(buf, true);
        chip_.save_restore(st);
        intf_.saveRestore(st);
        return buf;
    }
    void deserialize(const std::vector<std::uint8_t>& data) override {
        std::vector<std::uint8_t> buf = data;
        ymfm::ymfm_saved_state st(buf, false);
        chip_.save_restore(st);
        intf_.saveRestore(st);
    }

private:
    Interface intf_;
    MaskedYm2612 chip_;
};

}  // namespace

std::unique_ptr<IFmSynth> makeYmfmSynth() {
    return std::make_unique<YmfmSynth>();
}

}  // namespace gen::audio
