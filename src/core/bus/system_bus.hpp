#pragma once
// The 68000's system bus (spec 04 "68000 memory map", "Bus requirements", M02-01). A 256-entry table of
// 64 KB pages routes every access to the cartridge ROM, work RAM, the Z80 area, the I/O / control registers
// or the VDP ports. ROM and RAM pages are exposed as Bus68k fast pages (B-1); the CPU raises onAccess for
// every access itself (B-2). Accesses that hit nothing are recorded in an in-memory issue log.

#include <array>
#include <cstdint>
#include <vector>

#include "core/bus/io.hpp"
#include "core/m68k/m68k.hpp"
#include "core/vdp/vdp.hpp"
#include "core/z80/z80_control.hpp"

namespace gen::bus {

inline constexpr std::size_t kWorkRamSize = 0x10000;

enum class PageKind : std::uint8_t { Unmapped, Rom, Ram, Z80, Io, Vdp };

enum class BusIssue : std::uint8_t {
    Unmapped,     // nothing decodes the address (open bus on read, dropped on write)
    RomWrite,     // write to cartridge ROM (dropped)
    Z80NotOwned,  // 68k touched Z80 space without holding the Z80 bus (B-3)
    VdpLocked,    // VDP access while TMSS has not been unlocked
};
inline constexpr int kBusIssueCount = 4;
const char* toString(BusIssue issue);

struct BusIssueRecord {
    std::uint32_t addr = 0;
    std::uint32_t pc = 0;  // instruction making the access
    std::uint32_t value = 0;
    std::uint8_t size = 0;
    bool write = false;
    BusIssue issue = BusIssue::Unmapped;
};

// Master-clock position the bus needs for the VDP status / HV counter; owned and advanced by the Machine.
struct BusClock {
    std::uint64_t cpuMclk = 0;    // 68k time at the start of the executing instruction
    std::uint64_t lineStart = 0;  // start of the current scanline
};

struct BusState {
    std::array<std::uint8_t, kWorkRamSize> ram{};
    std::uint32_t tmssRegister = 0;  // $A14000 (long)
    std::uint8_t sramControl = 0;    // $A130F1
};

class SystemBus final : public m68k::Bus68k {
public:
    SystemBus(vdp::Vdp& vdp, Io& io, z80::Z80Control& z80, const BusClock& clock);

    // `rom` must outlive the bus. Rebuilds the page table.
    void attach(const std::vector<std::uint8_t>& rom, m68k::M68k& cpu, bool tmss);
    void reset();

    std::uint8_t read8(std::uint32_t addr) override;
    std::uint16_t read16(std::uint32_t addr) override;
    void write8(std::uint32_t addr, std::uint8_t value) override;
    void write16(std::uint32_t addr, std::uint16_t value) override;
    int iack(int level) override;

    // Recomputes the 68k IPL from the VDP (after line events and VDP register writes).
    void updateIrq();

    // Called before the 68k changes BUSREQ / RESET so the Z80 can first run up to the current time.
    void setZ80Sync(void (*fn)(void*), void* ctx) {
        z80Sync_ = fn;
        z80SyncCtx_ = ctx;
    }

    PageKind pageKind(std::uint32_t addr) const { return pages_[(addr >> 16) & 0xFF]; }
    bool vdpLocked() const { return tmss_ && state_.tmssRegister != 0x53454741; }  // 'SEGA'

    BusState& state() { return state_; }
    const BusState& state() const { return state_; }

    // Issue log: the first kIssueLogLimit records plus per-kind totals.
    static constexpr std::size_t kIssueLogLimit = 4096;
    const std::vector<BusIssueRecord>& issues() const { return issues_; }
    std::uint64_t issueCount(BusIssue issue) const { return issueCounts_[static_cast<std::size_t>(issue)]; }
    void clearIssues();

private:
    std::uint16_t openBus() const;
    void report(BusIssue issue, std::uint32_t addr, std::uint8_t size, std::uint32_t value, bool write);
    int lineMclk() const { return static_cast<int>(clock_.cpuMclk - clock_.lineStart); }

    std::uint16_t readZ80(std::uint32_t addr, std::uint8_t size);
    void writeZ80(std::uint32_t addr, std::uint8_t value, std::uint8_t size);
    std::uint16_t readIo(std::uint32_t addr, std::uint8_t size);
    void writeIo(std::uint32_t addr, std::uint16_t value, std::uint8_t size);
    std::uint16_t readVdp(std::uint32_t addr, std::uint8_t size);
    void writeVdp(std::uint32_t addr, std::uint16_t value, std::uint8_t size);
    static std::uint16_t dmaRead(void* ctx, std::uint32_t addr);

    vdp::Vdp& vdp_;
    Io& io_;
    z80::Z80Control& z80_;
    const BusClock& clock_;
    m68k::M68k* cpu_ = nullptr;
    const std::vector<std::uint8_t>* rom_ = nullptr;
    bool tmss_ = false;
    std::array<PageKind, 256> pages_{};
    BusState state_;
    std::vector<BusIssueRecord> issues_;
    std::array<std::uint64_t, kBusIssueCount> issueCounts_{};
    void (*z80Sync_)(void*) = nullptr;
    void* z80SyncCtx_ = nullptr;
};

}  // namespace gen::bus
