#include "core/audio/register_log.hpp"

#include <cstdio>

namespace gen::audio {

std::string RegisterLog::toCsv() const {
    std::string out = "frame,line,cpu,pc,chip,port,reg,value\n";
    char buf[96];
    for (std::size_t i = 0; i < size_; ++i) {
        const RegWrite& w = at(i);
        std::snprintf(buf, sizeof buf, "%llu,%u,%s,%06X,%s,%u,%02X,%02X\n",
                      static_cast<unsigned long long>(w.frame), w.line, w.cpu == CpuId::Z80 ? "z80" : "m68k",
                      w.pc, w.chip == Chip::Psg ? "psg" : "ym2612", w.port, w.reg, w.value);
        out += buf;
    }
    return out;
}

}  // namespace gen::audio
