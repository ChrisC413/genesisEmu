#pragma once
// Sound-chip register write log (spec 06 A-2): a ring buffer of every YM2612 / PSG write with the frame, line,
// CPU and PC that made it. Used to reverse engineer the SMPS drivers; exported as CSV (M05-05).

#include <cstdint>
#include <string>
#include <vector>

#include "core/hooks.hpp"

namespace gen::audio {

enum class Chip : std::uint8_t { Ym2612, Psg };

struct RegWrite {
    std::uint64_t frame = 0;
    std::uint16_t line = 0;
    CpuId cpu = CpuId::M68k;
    Chip chip = Chip::Ym2612;
    std::uint8_t port = 0;  // YM2612: register bank (0 = $4000/1, 1 = $4002/3); PSG: 0
    std::uint8_t reg = 0;   // YM2612: register; PSG: latched register (channel * 2 + volume bit)
    std::uint8_t value = 0;
    std::uint32_t pc = 0;
};

class RegisterLog {
public:
    explicit RegisterLog(std::size_t capacity = 1 << 16) : entries_(capacity) {}

    // Position stamped onto subsequent writes; the Machine updates it at every line start.
    void setPosition(std::uint64_t frame, int line) {
        frame_ = frame;
        line_ = static_cast<std::uint16_t>(line);
    }
    void setEnabled(bool on) { enabled_ = on; }
    bool enabled() const { return enabled_; }

    void push(CpuId cpu, Chip chip, std::uint8_t port, std::uint8_t reg, std::uint8_t value, std::uint32_t pc) {
        if (!enabled_ || entries_.empty()) {
            return;
        }
        entries_[head_] = RegWrite{frame_, line_, cpu, chip, port, reg, value, pc};
        head_ = (head_ + 1) % entries_.size();
        if (size_ < entries_.size()) {
            ++size_;
        }
        ++total_;
    }

    std::size_t size() const { return size_; }
    std::uint64_t total() const { return total_; }  // all writes since the last clear, including overwritten
    // i = 0 is the oldest retained entry.
    const RegWrite& at(std::size_t i) const {
        return entries_[(head_ + entries_.size() - size_ + i) % entries_.size()];
    }
    void clear() {
        head_ = 0;
        size_ = 0;
        total_ = 0;
    }

    // CSV with a header row: frame,line,cpu,pc,chip,port,reg,value (hex for pc / reg / value).
    std::string toCsv() const;

private:
    std::vector<RegWrite> entries_;
    std::size_t head_ = 0;
    std::size_t size_ = 0;
    std::uint64_t total_ = 0;
    std::uint64_t frame_ = 0;
    std::uint16_t line_ = 0;
    bool enabled_ = true;
};

}  // namespace gen::audio
