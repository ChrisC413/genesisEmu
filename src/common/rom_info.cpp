#include "common/rom_info.hpp"

#include <cstdio>

namespace gen::common {

namespace {

std::string hex(std::uint32_t value, int digits, const char* prefix = "$") {
    char buf[16];
    std::snprintf(buf, sizeof buf, "%s%0*X", prefix, digits, value);
    return buf;
}

std::string range(const cart::AddressRange& r) {
    return hex(r.start, 6) + "-" + hex(r.end, 6);
}

std::string identification(const cart::Rom& rom) {
    if (rom.known == nullptr) {
        return "unknown";
    }
    return std::string(rom.known->title) + " " + std::string(rom.known->revision) + " (" +
           std::string(rom.known->region) + ")";
}

std::string sram(const cart::SramDeclaration& s) {
    if (!s.declared) {
        return "none";
    }
    return range(s.range) + ", " + std::string(cart::toString(s.lanes)) + " bytes" +
           (s.backup ? ", battery-backed" : "");
}

}  // namespace

std::vector<RomInfoField> describeRom(const cart::Rom& rom) {
    const cart::RomHeader& h = rom.header;
    const std::string checksum =
        hex(h.checksum, 4) +
        (rom.checksumMatches() ? " (valid)" : " (mismatch: computed " + hex(rom.computedChecksum, 4) + ")");
    return {
        {"Identified", identification(rom)},
        {"SHA-1", rom.sha1Hex},
        {"CRC32", hex(rom.crc32, 8, "")},
        {"Size", std::to_string(rom.data.size()) + " bytes"},
        {"Format", std::string(cart::toString(rom.sourceFormat))},
        {"Console", h.consoleName},
        {"Copyright", h.copyright},
        {"Domestic title", h.domesticTitle},
        {"Overseas title", h.overseasTitle},
        {"Serial", h.serial},
        {"Checksum", checksum},
        {"I/O support", h.ioSupport},
        {"ROM range", range(h.romRange)},
        {"RAM range", range(h.ramRange)},
        {"SRAM", sram(h.sram)},
        {"Region", h.regionCodes + " (" + cart::regionsToString(h.regions) + ")"},
    };
}

nlohmann::json romToJson(const cart::Rom& rom) {
    const cart::RomHeader& h = rom.header;
    nlohmann::json j = {
        {"sha1", rom.sha1Hex},
        {"crc32", hex(rom.crc32, 8, "")},
        {"size", rom.data.size()},
        {"format", rom.sourceFormat == cart::RomFormat::Smd ? "smd" : "bin"},
        {"header",
         {
             {"console", h.consoleName},
             {"copyright", h.copyright},
             {"domesticTitle", h.domesticTitle},
             {"overseasTitle", h.overseasTitle},
             {"serial", h.serial},
             {"checksum", h.checksum},
             {"computedChecksum", rom.computedChecksum},
             {"checksumValid", rom.checksumMatches()},
             {"ioSupport", h.ioSupport},
             {"romRange", {h.romRange.start, h.romRange.end}},
             {"ramRange", {h.ramRange.start, h.ramRange.end}},
             {"regionCodes", h.regionCodes},
             {"regions", cart::regionsToString(h.regions)},
         }},
        {"warnings", rom.warnings},
    };
    if (h.sram.declared) {
        j["header"]["sram"] = {{"start", h.sram.range.start},
                               {"end", h.sram.range.end},
                               {"lanes", cart::toString(h.sram.lanes)},
                               {"backup", h.sram.backup}};
    } else {
        j["header"]["sram"] = nullptr;
    }
    if (rom.known != nullptr) {
        j["identified"] = {{"game", cart::toString(rom.known->game)},
                           {"title", rom.known->title},
                           {"revision", rom.known->revision},
                           {"region", rom.known->region}};
    } else {
        j["identified"] = nullptr;
    }
    return j;
}

}  // namespace gen::common
