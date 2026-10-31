#include "core/cart/sram.hpp"

#include <algorithm>

namespace gen::cart {

SramDevice::SramDevice(const SramDeclaration& decl, std::size_t romSize)
    : lanes_(decl.lanes), first_(decl.range.start), last_(decl.range.end) {
    if (lanes_ != SramLanes::Both) {
        first_ &= ~1u;
        last_ |= 1u;
    }
    const std::uint32_t span = last_ >= first_ ? last_ - first_ + 1 : 0;
    data_.assign(lanes_ == SramLanes::Both ? span : span / 2, 0xFF);
    powerOnControl_ = romSize > first_ ? 0x00 : 0x01;
    control_ = powerOnControl_;
}

void SramDevice::reset() {
    control_ = powerOnControl_;
}

bool SramDevice::timeRead(std::uint8_t reg, std::uint8_t& out) {
    if (reg != 0xF1) {
        return false;
    }
    out = control_;
    return true;
}

bool SramDevice::timeWrite(std::uint8_t reg, std::uint8_t value) {
    if (reg != 0xF1) {
        return false;
    }
    const bool wasMapped = mapped();
    control_ = static_cast<std::uint8_t>(value & 0x03);
    if (wasMapped && !mapped() && dirty_) {
        flushRequested_ = true;
    }
    return true;
}

bool SramDevice::index(std::uint32_t addr, std::size_t& out) const {
    if (addr < first_ || addr > last_) {
        return false;
    }
    const std::uint32_t off = addr - first_;
    switch (lanes_) {
        case SramLanes::Both:
            out = off;
            return true;
        case SramLanes::Even:
            out = off >> 1;
            return (addr & 1) == 0;
        case SramLanes::Odd:
            out = off >> 1;
            return (addr & 1) != 0;
    }
    return false;
}

bool SramDevice::memRead(std::uint32_t addr, std::uint8_t& out) {
    std::size_t i = 0;
    if (!mapped() || !index(addr, i) || i >= data_.size()) {
        return false;
    }
    out = data_[i];
    return true;
}

bool SramDevice::memWrite(std::uint32_t addr, std::uint8_t value) {
    std::size_t i = 0;
    if (!mapped() || !index(addr, i) || i >= data_.size()) {
        return false;
    }
    if (!writeProtected() && data_[i] != value) {
        data_[i] = value;
        dirty_ = true;
    }
    return true;
}

void SramDevice::load(std::span<const std::uint8_t> bytes) {
    std::copy_n(bytes.begin(), std::min(bytes.size(), data_.size()), data_.begin());
    dirty_ = false;
    flushRequested_ = false;
}

void SramDevice::hashState(state::StateHasher& h) const {
    h.tag("SRAM");
    h.u8(control_);
    h.bytes(data_);
}

}  // namespace gen::cart
