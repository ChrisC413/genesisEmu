#pragma once
// Cartridge device chain (spec 04 "Cartridge device chain", M04-08). A cartridge is its ROM plus an ordered
// list of devices that can claim /TIME registers ($A130xx), overlay part of the cartridge address space
// (SRAM) and snoop writes to ROM space. The first device that answers wins. Devices are part of the machine
// state (hash now, save states in M06).

#include <cstdint>
#include <string_view>

#include "core/state/state_hash.hpp"

namespace gen::cart {

class ICartDevice {
public:
    virtual ~ICartDevice() = default;
    virtual std::string_view name() const = 0;

    // /TIME register access; `reg` is the low byte of $A130xx. Return true when the device claims it.
    virtual bool timeRead(std::uint8_t reg, std::uint8_t& out) {
        (void)reg;
        (void)out;
        return false;
    }
    virtual bool timeWrite(std::uint8_t reg, std::uint8_t value) {
        (void)reg;
        (void)value;
        return false;
    }

    // Cartridge-space overlay: [first, last] of the 68k addresses the device may answer (empty: first > last).
    // The bus routes every access in the 64 KB pages that range touches through the chain before the ROM.
    virtual std::uint32_t memFirst() const { return 1; }
    virtual std::uint32_t memLast() const { return 0; }
    // Return true when the device answered the read / consumed the write. Writes to plain ROM space are
    // offered to every device too (snooping), whether or not they overlay it.
    virtual bool memRead(std::uint32_t addr, std::uint8_t& out) {
        (void)addr;
        (void)out;
        return false;
    }
    virtual bool memWrite(std::uint32_t addr, std::uint8_t value) {
        (void)addr;
        (void)value;
        return false;
    }

    // Console reset. Non-volatile contents survive.
    virtual void reset() {}
    virtual void hashState(state::StateHasher& h) const { (void)h; }
};

}  // namespace gen::cart
