#pragma once
// Battery-backed SRAM controller (spec 04 "Cartridge loading", M04-08): the SRAM the header declares at $1B0
// plus the $A130F1 control register (bit 0 = SRAM mapped, bit 1 = write protect). When the ROM extends over
// the SRAM range (S3K: 4 MB with SRAM at $200001), the SRAM is unmapped at power on and the ROM shows through
// until the game enables it; otherwise it is mapped from power on.

#include <cstdint>
#include <span>
#include <vector>

#include "core/cart/cart_device.hpp"
#include "core/cart/rom.hpp"

namespace gen::cart {

class SramDevice final : public ICartDevice {
public:
    SramDevice(const SramDeclaration& decl, std::size_t romSize);

    std::string_view name() const override { return "sram"; }
    bool timeRead(std::uint8_t reg, std::uint8_t& out) override;
    bool timeWrite(std::uint8_t reg, std::uint8_t value) override;
    std::uint32_t memFirst() const override { return first_; }
    std::uint32_t memLast() const override { return last_; }
    bool memRead(std::uint32_t addr, std::uint8_t& out) override;
    bool memWrite(std::uint32_t addr, std::uint8_t value) override;
    void reset() override;
    void hashState(state::StateHasher& h) const override;
    // Contents and control register. Loading marks the SRAM dirty when its contents change, so the battery
    // file follows the loaded state.
    void serialize(state::Archive& ar) override;

    bool mapped() const { return (control_ & 0x01) != 0; }
    bool writeProtected() const { return (control_ & 0x02) != 0; }
    std::uint8_t control() const { return control_; }

    // Contents (one byte per SRAM cell, in address order) for persistence as <sha1>.srm.
    const std::vector<std::uint8_t>& data() const { return data_; }
    // Loads saved contents; a size mismatch loads the common prefix.
    void load(std::span<const std::uint8_t> bytes);
    // Written since the last clearDirty(); flushRequested() additionally reports a write to $A130F1 that
    // unmapped dirty SRAM (spec 04: persist on SRAM-disable writes).
    bool dirty() const { return dirty_; }
    bool flushRequested() const { return flushRequested_; }
    void clearDirty() {
        dirty_ = false;
        flushRequested_ = false;
    }

private:
    bool index(std::uint32_t addr, std::size_t& out) const;

    SramLanes lanes_ = SramLanes::Both;
    std::uint32_t first_ = 0;
    std::uint32_t last_ = 0;
    std::uint8_t powerOnControl_ = 0;
    std::uint8_t control_ = 0;
    std::vector<std::uint8_t> data_;
    bool dirty_ = false;
    bool flushRequested_ = false;
};

}  // namespace gen::cart
