#include "app/debugger_ui.hpp"

#include <algorithm>
#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <format>
#include <fstream>

#include <SDL3/SDL_opengl.h>
#include <imgui.h>

#include "core/sched/machine.hpp"

namespace gen::app {

namespace {

constexpr ImU32 kChanged = IM_COL32(255, 210, 90, 255);
constexpr ImU32 kDim = IM_COL32(140, 140, 150, 255);
constexpr ImU32 kPcRow = IM_COL32(70, 90, 160, 160);
constexpr ImU32 kBpDot = IM_COL32(220, 50, 50, 255);
constexpr ImU32 kBpDotOff = IM_COL32(220, 50, 50, 90);

std::optional<std::uint32_t> parseHex(const char* text) {
    std::string_view s(text);
    while (!s.empty() && s.front() == ' ') {
        s.remove_prefix(1);
    }
    while (!s.empty() && s.back() == ' ') {
        s.remove_suffix(1);
    }
    if (s.starts_with('$')) {
        s.remove_prefix(1);
    } else if (s.starts_with("0x") || s.starts_with("0X")) {
        s.remove_prefix(2);
    }
    if (s.empty() || s.size() > 8) {
        return std::nullopt;
    }
    std::uint32_t v = 0;
    for (const char c : s) {
        int d = -1;
        if (c >= '0' && c <= '9') {
            d = c - '0';
        } else if (c >= 'a' && c <= 'f') {
            d = c - 'a' + 10;
        } else if (c >= 'A' && c <= 'F') {
            d = c - 'A' + 10;
        }
        if (d < 0) {
            return std::nullopt;
        }
        v = v * 16 + static_cast<std::uint32_t>(d);
    }
    return v;
}

template <std::size_t N>
void setText(std::array<char, N>& buf, std::string_view text) {
    const std::size_t n = std::min(text.size(), N - 1);
    std::memcpy(buf.data(), text.data(), n);
    buf[n] = 0;
}

// Hex register field: editable while paused; highlighted when it changed since the previous stop.
template <typename T>
bool regField(const char* label, T& value, T previous, bool editable, bool highlight) {
    constexpr int digits = sizeof(T) * 2;
    ImGui::TextUnformatted(label);
    ImGui::SameLine(ImGui::GetFontSize() * 2.6f);
    const bool changed = highlight && value != previous;
    if (changed) {
        ImGui::PushStyleColor(ImGuiCol_Text, kChanged);
    }
    bool edited = false;
    ImGui::PushID(label);
    if (editable) {
        ImGui::SetNextItemWidth(ImGui::CalcTextSize("0").x * (digits + 2.5f));
        const ImGuiDataType type = sizeof(T) == 1   ? ImGuiDataType_U8
                                   : sizeof(T) == 2 ? ImGuiDataType_U16
                                                    : ImGuiDataType_U32;
        const char* fmt = sizeof(T) == 1 ? "%02X" : sizeof(T) == 2 ? "%04X" : "%08X";
        edited = ImGui::InputScalar("##v", type, &value, nullptr, nullptr, fmt,
                                    ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_EnterReturnsTrue);
    } else {
        ImGui::Text("%0*X", digits, static_cast<unsigned>(value));
    }
    ImGui::PopID();
    if (changed) {
        ImGui::PopStyleColor();
    }
    return edited;
}

const char* kindName(debug::BpKind k) {
    return debug::toString(k);
}

constexpr std::array<debug::BpKind, 12> kKinds{
    debug::BpKind::Exec,   debug::BpKind::Read,  debug::BpKind::Write,     debug::BpKind::Access,
    debug::BpKind::VdpReg, debug::BpKind::Vram,  debug::BpKind::Cram,      debug::BpKind::Vsram,
    debug::BpKind::Dma,    debug::BpKind::Interrupt, debug::BpKind::Exception, debug::BpKind::Frame};

}  // namespace

// ---- Texture -------------------------------------------------------------------------------------------

Texture::~Texture() {
    if (tex_ != 0) {
        const auto t = static_cast<GLuint>(tex_);
        glDeleteTextures(1, &t);
    }
}

void Texture::update(int width, int height, const std::uint32_t* rgba) {
    if (tex_ == 0) {
        GLuint t = 0;
        glGenTextures(1, &t);
        tex_ = t;
        glBindTexture(GL_TEXTURE_2D, t);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(tex_));
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    if (width != w_ || height != h_) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
        w_ = width;
        h_ = height;
    } else {
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    }
}

// ---- Memory view state ----------------------------------------------------------------------------------

struct DebuggerUi::MemoryView {
    bool open = false;
    int region = static_cast<int>(debug::Region::Ram68k);
    std::array<char, 16> gotoBuf{};
    std::array<char, 96> searchBuf{};
    std::vector<std::size_t> hits;
    int hitIndex = -1;
    std::string status;
    long long scrollTo = -1;
    long long editOffset = -1;
    std::array<char, 4> editBuf{};
    bool focusEdit = false;
    bool highlight = true;
    // Contents at the previous stop (what "changed" compares against) and at the latest stop.
    std::array<std::vector<std::uint8_t>, static_cast<std::size_t>(debug::Region::Count)> previous;
    std::array<std::vector<std::uint8_t>, static_cast<std::size_t>(debug::Region::Count)> latest;
};

DebuggerUi::DebuggerUi() {
    for (auto& v : memory_) {
        v = std::make_unique<MemoryView>();
    }
    memory_[1]->region = static_cast<int>(debug::Region::Rom);
    memory_[2]->region = static_cast<int>(debug::Region::Vram);
    memory_[3]->region = static_cast<int>(debug::Region::RamZ80);
    form_.kind = debug::BpKind::Exec;
}

DebuggerUi::~DebuggerUi() = default;

void DebuggerUi::setDefaultTracePath(std::string path) {
    if (tracePath_[0] == 0) {
        setText(tracePath_, path + "/trace.txt");
        setText(ringPath_, path + "/ring.txt");
    }
}

void DebuggerUi::attach(Machine* machine, debug::Debugger* debugger) {
    m_ = machine;
    d_ = debugger;
    known_.clear();
    patches_.clear();
    disasmTopValid_ = false;
    haveSnapshot_ = false;
    boxes_.clear();
    for (auto& v : memory_) {
        for (auto& p : v->previous) {
            p.clear();
        }
        for (auto& p : v->latest) {
            p.clear();
        }
        v->hits.clear();
        v->editOffset = -1;
    }
    if (m_ != nullptr) {
        onStop();
    }
}

void DebuggerUi::onStop() {
    if (m_ == nullptr) {
        return;
    }
    prev68k_ = haveSnapshot_ ? cur68k_ : m_->cpu().state();
    prevZ80_ = haveSnapshot_ ? curZ80_ : m_->z80Cpu().state();
    cur68k_ = m_->cpu().state();
    curZ80_ = m_->z80Cpu().state();
    haveSnapshot_ = true;
    known_.add(CpuId::M68k, cur68k_.pc);
    known_.add(CpuId::Z80, curZ80_.pc);
    for (const debug::CallFrame& f : d_->callStack()) {
        known_.add(CpuId::M68k, f.returnPc);
        known_.add(CpuId::M68k, f.callPc);
    }
    if (d_->ringEnabled()) {
        for (const debug::RingEntry& e : d_->ring()) {
            if (e.irq == 0) {
                known_.add(e.cpu, e.pc);
            }
        }
    }
    for (auto& v : memory_) {
        if (!v->open) {
            continue;
        }
        const auto r = static_cast<debug::Region>(v->region);
        const auto i = static_cast<std::size_t>(r);
        std::vector<std::uint8_t> now = debug::snapshot(*m_, r);
        v->previous[i] = v->latest[i].size() == now.size() ? std::move(v->latest[i]) : now;
        v->latest[i] = std::move(now);
    }
    if (followPc_) {
        disasmTopValid_ = false;
    }
}

std::vector<std::string> DebuggerUi::windowNames() {
    std::vector<std::string> names{kDebuggerWindow, kDisassemblyWindow, kM68kRegsWindow, kZ80RegsWindow,
                                   kBreakpointsWindow, kTraceWindow, kCallStackWindow, kVdpRegsWindow,
                                   kTilesWindow, kPlanesWindow, kSpritesWindow, kPaletteWindow};
    for (int i = 1; i <= kMemoryViews; ++i) {
        names.push_back(std::format("Memory {}", i));
    }
    return names;
}

void DebuggerUi::openAll() {
    showDebugger_ = showDisasm_ = show68k_ = showZ80_ = showBreakpoints_ = showTrace_ = showCallStack_ =
        showVdpRegs_ = showTiles_ = showPlanes_ = showSprites_ = showPalette_ = true;
    memory_[0]->open = true;
    memory_[1]->open = true;
}

bool DebuggerUi::anyOpen() const {
    return showDebugger_ || showDisasm_ || show68k_ || showZ80_ || showBreakpoints_ || showTrace_ ||
           showCallStack_ || showVdpRegs_ || showTiles_ || showPlanes_ || showSprites_ || showPalette_ ||
           std::any_of(memory_.begin(), memory_.end(), [](const auto& v) { return v->open; });
}

void DebuggerUi::drawMenu() {
    ImGui::MenuItem(kDebuggerWindow, nullptr, &showDebugger_);
    ImGui::MenuItem(kDisassemblyWindow, nullptr, &showDisasm_);
    ImGui::MenuItem(kM68kRegsWindow, nullptr, &show68k_);
    ImGui::MenuItem(kZ80RegsWindow, nullptr, &showZ80_);
    if (ImGui::BeginMenu("Memory")) {
        for (int i = 0; i < kMemoryViews; ++i) {
            ImGui::MenuItem(std::format("Memory {}", i + 1).c_str(), nullptr, &memory_[i]->open);
        }
        ImGui::EndMenu();
    }
    ImGui::MenuItem(kBreakpointsWindow, nullptr, &showBreakpoints_);
    ImGui::MenuItem(kCallStackWindow, nullptr, &showCallStack_);
    ImGui::MenuItem(kTraceWindow, nullptr, &showTrace_);
    ImGui::Separator();
    ImGui::MenuItem(kVdpRegsWindow, nullptr, &showVdpRegs_);
    ImGui::MenuItem(kTilesWindow, nullptr, &showTiles_);
    ImGui::MenuItem(kPlanesWindow, nullptr, &showPlanes_);
    ImGui::MenuItem(kSpritesWindow, nullptr, &showSprites_);
    ImGui::MenuItem(kPaletteWindow, nullptr, &showPalette_);
}

void DebuggerUi::draw(bool running) {
    if (m_ == nullptr) {
        return;
    }
    drawRunControl(running);
    drawDisassembly(running);
    drawM68kRegisters(running);
    drawZ80Registers(running);
    for (int i = 0; i < kMemoryViews; ++i) {
        drawMemory(i, running);
    }
    drawBreakpoints(running);
    drawTrace(running);
    drawCallStack(running);
    drawVdpRegisters();
    drawTiles();
    drawPlanes();
    drawSprites();
    drawPalette();
}

// ---- Run control (M06-01) -------------------------------------------------------------------------------

void DebuggerUi::drawRunControl(bool running) {
    if (!showDebugger_) {
        return;
    }
    if (!ImGui::Begin(kDebuggerWindow, &showDebugger_)) {
        ImGui::End();
        return;
    }
    const auto button = [](const char* label, const char* tip) {
        const bool pressed = ImGui::Button(label);
        ImGui::SetItemTooltip("%s", tip);
        ImGui::SameLine();
        return pressed;
    };
    if (button(running ? "Pause" : "Run", running ? "Pause (F5)" : "Run (F5)")) {
        request(running ? DebugCommand::Pause : DebugCommand::Run);
    }
    ImGui::BeginDisabled(running);
    if (button("Into", "Step into: one instruction of the focused CPU (F11)")) {
        request(DebugCommand::StepInto);
    }
    if (button("Over", "Step over a JSR / BSR / TRAP or CALL / RST (F10)")) {
        request(DebugCommand::StepOver);
    }
    if (button("Out", "Run until the current subroutine returns (Shift+F11)")) {
        request(DebugCommand::StepOut);
    }
    if (button("Line", "Run to the next scanline (Shift+F6)")) {
        request(DebugCommand::StepLine);
    }
    if (button("Frame", "Run to the next frame (F6)")) {
        request(DebugCommand::StepFrame);
    }
    ImGui::EndDisabled();
    if (button("Reset", "Soft reset: the reset button (68k only)")) {
        request(DebugCommand::SoftReset);
    }
    if (button("Power", "Hard reset: power cycle")) {
        request(DebugCommand::HardReset);
    }
    ImGui::NewLine();
    int focus = focus_ == CpuId::Z80 ? 1 : 0;
    ImGui::TextUnformatted("Focus:");
    ImGui::SameLine();
    ImGui::RadioButton("68000", &focus, 0);
    ImGui::SameLine();
    ImGui::RadioButton("Z80", &focus, 1);
    focus_ = focus == 1 ? CpuId::Z80 : CpuId::M68k;
    ImGui::Separator();
    ImGui::Text("%s | frame %llu  line %d  MCLK %llu", running ? "running" : "paused",
                static_cast<unsigned long long>(m_->frame()), m_->line(),
                static_cast<unsigned long long>(m_->cpuMclk()));
    ImGui::Text("68k PC $%06X   Z80 PC $%04X%s", m_->cpu().state().pc, m_->z80Cpu().state().pc,
                m_->z80().running() ? "" : " (held)");
    const debug::StopInfo& stop = d_->lastStop();
    if (!stop.text.empty()) {
        ImGui::TextColored(ImVec4(1, 0.85f, 0.4f, 1), "last stop: %s", stop.text.c_str());
    }
    if (d_->stepPending()) {
        ImGui::TextColored(ImVec4(0.6f, 0.8f, 1, 1), "step in progress (Pause cancels it)");
    }
    if (!d_->log().empty()) {
        ImGui::Separator();
        ImGui::Text("Logpoints (%zu)", d_->log().size());
        ImGui::SameLine();
        if (ImGui::SmallButton("Clear")) {
            d_->clearLog();
        }
        if (ImGui::BeginChild("log", ImVec2(0, 0), ImGuiChildFlags_Borders)) {
            ImGuiListClipper clip;
            clip.Begin(static_cast<int>(d_->log().size()));
            while (clip.Step()) {
                for (int i = clip.DisplayStart; i < clip.DisplayEnd; ++i) {
                    ImGui::TextUnformatted(d_->log()[static_cast<std::size_t>(i)].c_str());
                }
            }
            if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
                ImGui::SetScrollHereY(1.0f);
            }
        }
        ImGui::EndChild();
    }
    ImGui::End();
}

// ---- Disassembly (M06-10) -------------------------------------------------------------------------------

void DebuggerUi::gotoDisassembly(CpuId cpu, std::uint32_t addr) {
    focus_ = cpu;
    followPc_ = false;
    disasmTop_ = debug::previousStart(*m_, cpu, addr, known_);
    for (int i = 0; i < 4; ++i) {
        disasmTop_ = debug::previousStart(*m_, cpu, disasmTop_, known_);
    }
    disasmTopValid_ = true;
    showDisasm_ = true;
}

void DebuggerUi::drawDisassembly(bool running) {
    if (!showDisasm_) {
        return;
    }
    if (!ImGui::Begin(kDisassemblyWindow, &showDisasm_)) {
        ImGui::End();
        return;
    }
    const CpuId cpu = focus_;
    const std::uint32_t pc = cpu == CpuId::Z80 ? m_->z80Cpu().state().pc : m_->cpu().state().pc;
    ImGui::Checkbox("Follow PC", &followPc_);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 6);
    if (ImGui::InputTextWithHint("##goto", "go to $", disasmGoto_.data(), disasmGoto_.size(),
                                 ImGuiInputTextFlags_EnterReturnsTrue)) {
        if (const auto a = parseHex(disasmGoto_.data())) {
            gotoDisassembly(cpu, *a);
        }
    }
    ImGui::SameLine();
    ImGui::TextDisabled("%s  click the gutter: breakpoint, right-click: run to", cpu == CpuId::Z80 ? "Z80" : "68000");
    const float lineH = ImGui::GetTextLineHeightWithSpacing();
    if (ImGui::BeginChild("listing", ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar)) {
        const int rows = std::max(4, static_cast<int>(ImGui::GetContentRegionAvail().y / lineH));
        const std::uint32_t idx = cpu == CpuId::Z80 ? 1 : 0;
        if (followPc_ && (!disasmTopValid_ || pc != lastPc_[idx])) {
            // Keep the PC line visible: re-anchor when it left the window.
            const auto visible = disasmTopValid_ ? debug::buildListing(*m_, cpu, disasmTop_, 0, rows - 2, known_)
                                                 : std::vector<debug::ListingLine>{};
            const bool inside = std::any_of(visible.begin(), visible.end(),
                                            [&](const debug::ListingLine& l) { return l.addr == pc; });
            if (!inside) {
                disasmTop_ = pc;
                for (int i = 0; i < rows / 3; ++i) {
                    disasmTop_ = debug::previousStart(*m_, cpu, disasmTop_, known_);
                }
            }
            disasmTopValid_ = true;
        }
        lastPc_[idx] = pc;
        if (!disasmTopValid_) {
            disasmTop_ = pc;
            disasmTopValid_ = true;
        }
        // Mouse wheel scrolls by instructions (backwards resynchronizes on known starts).
        if (ImGui::IsWindowHovered() && ImGui::GetIO().MouseWheel != 0) {
            followPc_ = false;
            const int steps = static_cast<int>(-ImGui::GetIO().MouseWheel * 3);
            for (int i = 0; i < std::abs(steps); ++i) {
                if (steps > 0) {
                    disasmTop_ += debug::disassembleAt(*m_, cpu, disasmTop_).length;
                } else {
                    disasmTop_ = debug::previousStart(*m_, cpu, disasmTop_, known_);
                }
            }
        }
        const auto lines = debug::buildListing(*m_, cpu, disasmTop_, 0, rows, known_);
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const float gutter = ImGui::GetFontSize() * 1.2f;
        for (const debug::ListingLine& l : lines) {
            const ImVec2 at = ImGui::GetCursorScreenPos();
            const float width = ImGui::GetContentRegionAvail().x;
            if (l.addr == pc) {
                dl->AddRectFilled(at, ImVec2(at.x + width, at.y + lineH), kPcRow);
            }
            const debug::Breakpoint* bp = nullptr;
            for (const debug::Breakpoint& b : d_->breakpoints()) {
                if (b.kind == debug::BpKind::Exec && b.cpu == cpu && l.addr >= b.start && l.addr <= b.end) {
                    bp = &b;
                    break;
                }
            }
            if (bp != nullptr) {
                dl->AddCircleFilled(ImVec2(at.x + gutter * 0.45f, at.y + lineH * 0.5f), lineH * 0.3f,
                                    bp->enabled ? kBpDot : kBpDotOff);
            }
            ImGui::PushID(static_cast<int>(l.addr));
            if (ImGui::InvisibleButton("gutter", ImVec2(gutter, lineH))) {
                d_->toggleExec(cpu, l.addr);
            }
            ImGui::SameLine(0, 0);
            const std::string row = cpu == CpuId::Z80 ? std::format("{:04X}  {:<12} {}", l.addr, l.bytes, l.text)
                                                      : std::format("{:06X}  {:<25} {}", l.addr, l.bytes, l.text);
            ImGui::PushStyleColor(ImGuiCol_Text, l.valid ? ImGui::GetColorU32(ImGuiCol_Text) : kDim);
            ImGui::Selectable(row.c_str(), false, ImGuiSelectableFlags_AllowOverlap, ImVec2(0, lineH - 2));
            ImGui::PopStyleColor();
            if (ImGui::BeginPopupContextItem("line")) {
                if (ImGui::MenuItem("Run to cursor", nullptr, false, !running)) {
                    runTo_ = l.addr;
                    request(DebugCommand::RunTo);
                }
                if (ImGui::MenuItem("Toggle breakpoint")) {
                    d_->toggleExec(cpu, l.addr);
                }
                if (ImGui::MenuItem("Set PC here", nullptr, false, !running)) {
                    if (cpu == CpuId::Z80) {
                        m_->z80Cpu().state().pc = static_cast<std::uint16_t>(l.addr);
                    } else {
                        m_->cpu().state().pc = l.addr;
                        m_->cpu().refillPrefetch();
                    }
                    onStop();
                }
                ImGui::EndPopup();
            }
            ImGui::PopID();
        }
    }
    ImGui::EndChild();
    ImGui::End();
}

// ---- CPU registers (M06-05) -----------------------------------------------------------------------------

void DebuggerUi::drawM68kRegisters(bool running) {
    if (!show68k_) {
        return;
    }
    if (!ImGui::Begin(kM68kRegsWindow, &show68k_)) {
        ImGui::End();
        return;
    }
    m68k::State& s = m_->cpu().state();
    const bool edit = !running;
    const bool hl = haveSnapshot_ && !running;
    bool changed = false;
    if (ImGui::BeginTable("regs", 2, ImGuiTableFlags_SizingStretchSame)) {
        for (std::size_t i = 0; i < 8; ++i) {
            ImGui::TableNextColumn();
            changed |= regField(std::format("D{}", i).c_str(), s.d[i], prev68k_.d[i], edit, hl);
            ImGui::TableNextColumn();
            changed |= regField(std::format("A{}", i).c_str(), s.a[i], prev68k_.a[i], edit, hl);
        }
        ImGui::EndTable();
    }
    ImGui::Separator();
    std::uint32_t pc = s.pc;
    if (regField("PC", pc, prev68k_.pc, edit, hl)) {
        s.pc = pc & 0xFFFFFE;
        m_->cpu().refillPrefetch();  // the prefetch queue follows the new PC
        changed = true;
    }
    std::uint32_t usp = s.userSp();
    std::uint32_t ssp = s.supervisorSp();
    if (regField("USP", usp, prev68k_.userSp(), edit, hl)) {
        s.setUserSp(usp);
        changed = true;
    }
    if (regField("SSP", ssp, prev68k_.supervisorSp(), edit, hl)) {
        s.setSupervisorSp(ssp);
        changed = true;
    }
    std::uint16_t sr = s.sr;
    if (regField("SR", sr, prev68k_.sr, edit, hl)) {
        s.sr = static_cast<std::uint16_t>(sr & m68k::kSrValidBits);
        changed = true;
    }
    // Flags as checkboxes.
    struct Flag {
        const char* name;
        std::uint16_t bit;
    };
    constexpr std::array<Flag, 7> flags{{{"T", m68k::kSrTrace},
                                         {"S", m68k::kSrSupervisor},
                                         {"X", m68k::kSrExtend},
                                         {"N", m68k::kSrNegative},
                                         {"Z", m68k::kSrZero},
                                         {"V", m68k::kSrOverflow},
                                         {"C", m68k::kSrCarry}}};
    ImGui::BeginDisabled(!edit);
    for (const Flag& f : flags) {
        bool on = (s.sr & f.bit) != 0;
        const bool flagChanged = hl && ((s.sr ^ prev68k_.sr) & f.bit) != 0;
        if (flagChanged) {
            ImGui::PushStyleColor(ImGuiCol_Text, kChanged);
        }
        if (ImGui::Checkbox(f.name, &on)) {
            if (f.bit == m68k::kSrSupervisor) {
                // Switching modes swaps the active stack pointer.
                const std::uint32_t u = s.userSp();
                const std::uint32_t sv = s.supervisorSp();
                s.sr = static_cast<std::uint16_t>(on ? s.sr | f.bit : s.sr & ~f.bit);
                s.setUserSp(u);
                s.setSupervisorSp(sv);
            } else {
                s.sr = static_cast<std::uint16_t>(on ? s.sr | f.bit : s.sr & ~f.bit);
            }
            changed = true;
        }
        if (flagChanged) {
            ImGui::PopStyleColor();
        }
        ImGui::SameLine();
    }
    int mask = (s.sr >> 8) & 7;
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 4);
    if (ImGui::SliderInt("IPL mask", &mask, 0, 7)) {
        s.sr = static_cast<std::uint16_t>((s.sr & ~m68k::kSrIntMask) | (mask << 8));
        changed = true;
    }
    ImGui::EndDisabled();
    ImGui::Text("IR %04X  IRC %04X  IPL in %d%s%s", s.ir, s.irc, m_->cpu().irqLevel(),
                s.stopped ? "  STOPPED" : "", s.halted ? "  HALTED" : "");
    if (running) {
        ImGui::TextDisabled("Pause to edit registers.");
    }
    if (changed) {
        cur68k_ = s;  // edits are not "changes since the last stop"
        prev68k_ = s;
    }
    ImGui::End();
}

void DebuggerUi::drawZ80Registers(bool running) {
    if (!showZ80_) {
        return;
    }
    if (!ImGui::Begin(kZ80RegsWindow, &showZ80_)) {
        ImGui::End();
        return;
    }
    z80::Z80State& z = m_->z80Cpu().state();
    const z80::Z80State& p = prevZ80_;
    const bool edit = !running;
    const bool hl = haveSnapshot_ && !running;
    bool changed = false;
    const auto pairField = [&](const char* name, std::uint8_t& hi, std::uint8_t& lo, std::uint8_t phi,
                               std::uint8_t plo) {
        auto v = static_cast<std::uint16_t>((hi << 8) | lo);
        const auto pv = static_cast<std::uint16_t>((phi << 8) | plo);
        if (regField(name, v, pv, edit, hl)) {
            hi = static_cast<std::uint8_t>(v >> 8);
            lo = static_cast<std::uint8_t>(v);
            changed = true;
        }
    };
    if (ImGui::BeginTable("z80", 2, ImGuiTableFlags_SizingStretchSame)) {
        ImGui::TableNextColumn();
        pairField("AF", z.a, z.f, p.a, p.f);
        ImGui::TableNextColumn();
        changed |= regField("AF'", z.af2, p.af2, edit, hl);
        ImGui::TableNextColumn();
        pairField("BC", z.b, z.c, p.b, p.c);
        ImGui::TableNextColumn();
        changed |= regField("BC'", z.bc2, p.bc2, edit, hl);
        ImGui::TableNextColumn();
        pairField("DE", z.d, z.e, p.d, p.e);
        ImGui::TableNextColumn();
        changed |= regField("DE'", z.de2, p.de2, edit, hl);
        ImGui::TableNextColumn();
        pairField("HL", z.h, z.l, p.h, p.l);
        ImGui::TableNextColumn();
        changed |= regField("HL'", z.hl2, p.hl2, edit, hl);
        ImGui::TableNextColumn();
        changed |= regField("IX", z.ix, p.ix, edit, hl);
        ImGui::TableNextColumn();
        changed |= regField("IY", z.iy, p.iy, edit, hl);
        ImGui::TableNextColumn();
        changed |= regField("SP", z.sp, p.sp, edit, hl);
        ImGui::TableNextColumn();
        changed |= regField("PC", z.pc, p.pc, edit, hl);
        ImGui::TableNextColumn();
        changed |= regField("I", z.i, p.i, edit, hl);
        ImGui::TableNextColumn();
        changed |= regField("R", z.r, p.r, edit, hl);
        ImGui::EndTable();
    }
    const std::uint8_t f = z.f;
    ImGui::Text("Flags  %c %c %c %c %c %c", (f & 0x80) ? 'S' : '-', (f & 0x40) ? 'Z' : '-', (f & 0x10) ? 'H' : '-',
                (f & 0x04) ? 'P' : '-', (f & 0x02) ? 'N' : '-', (f & 0x01) ? 'C' : '-');
    ImGui::BeginDisabled(!edit);
    changed |= ImGui::Checkbox("IFF1", &z.iff1);
    ImGui::SameLine();
    changed |= ImGui::Checkbox("IFF2", &z.iff2);
    ImGui::SameLine();
    changed |= ImGui::Checkbox("Halted", &z.halted);
    int im = z.im;
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 4);
    if (ImGui::SliderInt("IM", &im, 0, 2)) {
        z.im = static_cast<std::uint8_t>(im);
        changed = true;
    }
    ImGui::EndDisabled();
    const auto& c = m_->z80().state();
    ImGui::Text("BUSREQ %s  RESET %s  %s  bank $%06X", c.busRequested ? "on" : "off", c.resetAsserted ? "on" : "off",
                m_->z80().running() ? "running" : "held", static_cast<unsigned>(c.bank) << 15);
    ImGui::Text("INT line %s", z.irqLine ? "asserted" : "clear");
    if (running) {
        ImGui::TextDisabled("Pause to edit registers.");
    }
    if (changed) {
        curZ80_ = z;
        prevZ80_ = z;
    }
    ImGui::End();
}

// ---- Memory (M06-06) ------------------------------------------------------------------------------------

void DebuggerUi::drawMemory(int index, bool running) {
    MemoryView& v = *memory_[static_cast<std::size_t>(index)];
    if (!v.open) {
        return;
    }
    const std::string title = std::format("Memory {}", index + 1);
    if (!ImGui::Begin(title.c_str(), &v.open)) {
        ImGui::End();
        return;
    }
    const auto region = static_cast<debug::Region>(v.region);
    const debug::RegionInfo info = debug::regionInfo(region);
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 7);
    if (ImGui::BeginCombo("##region", info.name)) {
        for (int r = 0; r < static_cast<int>(debug::Region::Count); ++r) {
            const auto rr = static_cast<debug::Region>(r);
            if (debug::regionSize(*m_, rr) == 0) {
                continue;
            }
            if (ImGui::Selectable(debug::regionInfo(rr).name, r == v.region)) {
                v.region = r;
                v.hits.clear();
                v.editOffset = -1;
                v.status.clear();
            }
        }
        ImGui::EndCombo();
    }
    const std::size_t size = debug::regionSize(*m_, region);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 6);
    if (ImGui::InputTextWithHint("##goto", "go to $", v.gotoBuf.data(), v.gotoBuf.size(),
                                 ImGuiInputTextFlags_EnterReturnsTrue)) {
        if (const auto a = parseHex(v.gotoBuf.data())) {
            std::uint32_t off = *a;
            if (off >= info.displayBase && info.displayBase != 0) {
                off -= info.displayBase;
            }
            if (region == debug::Region::Ram68k) {
                off &= 0xFFFF;
            }
            if (off < size) {
                v.scrollTo = off;
            } else {
                v.status = "address outside " + std::string(info.name);
            }
        }
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 10);
    if (ImGui::InputTextWithHint("##search", "search bytes: 4E 75 ?? 00", v.searchBuf.data(), v.searchBuf.size(),
                                 ImGuiInputTextFlags_EnterReturnsTrue)) {
        std::string error;
        if (const auto pat = debug::parsePattern(v.searchBuf.data(), error)) {
            v.hits = debug::findAll(*m_, region, *pat);
            v.hitIndex = v.hits.empty() ? -1 : 0;
            v.status = std::format("{} match{}", v.hits.size(), v.hits.size() == 1 ? "" : "es");
            if (!v.hits.empty()) {
                v.scrollTo = static_cast<long long>(v.hits[0]);
            }
        } else {
            v.status = error;
            v.hits.clear();
        }
    }
    if (!v.hits.empty()) {
        ImGui::SameLine();
        if (ImGui::ArrowButton("##prev", ImGuiDir_Left)) {
            v.hitIndex = (v.hitIndex + static_cast<int>(v.hits.size()) - 1) % static_cast<int>(v.hits.size());
            v.scrollTo = static_cast<long long>(v.hits[static_cast<std::size_t>(v.hitIndex)]);
        }
        ImGui::SameLine();
        if (ImGui::ArrowButton("##next", ImGuiDir_Right)) {
            v.hitIndex = (v.hitIndex + 1) % static_cast<int>(v.hits.size());
            v.scrollTo = static_cast<long long>(v.hits[static_cast<std::size_t>(v.hitIndex)]);
        }
        ImGui::SameLine();
        ImGui::Text("%d/%zu", v.hitIndex + 1, v.hits.size());
    }
    ImGui::SameLine();
    ImGui::Checkbox("changes", &v.highlight);
    if (!v.status.empty()) {
        ImGui::TextDisabled("%s", v.status.c_str());
    }
    if (region == debug::Region::Rom && !patches_.empty()) {
        ImGui::TextColored(ImVec4(1, 0.6f, 0.4f, 1), "%zu ROM byte(s) patched (lost when the ROM is reloaded)",
                           patches_.size());
    }

    const auto& prev = v.previous[static_cast<std::size_t>(region)];
    constexpr int kCols = 16;
    const int rows = static_cast<int>((size + kCols - 1) / kCols);
    const float charW = ImGui::CalcTextSize("0").x;
    const float lineH = ImGui::GetTextLineHeightWithSpacing();
    if (ImGui::BeginChild("hex", ImVec2(0, 0), ImGuiChildFlags_Borders)) {
        if (v.scrollTo >= 0) {
            ImGui::SetScrollY(static_cast<float>(v.scrollTo / kCols) * lineH);
            v.scrollTo = -1;
        }
        const int addrDigits = info.displayBase + size > 0x10000 ? 6 : 4;
        const float hexX = charW * static_cast<float>(addrDigits + 2);
        const float asciiX = hexX + charW * 3 * kCols + charW;
        ImGuiListClipper clip;
        clip.Begin(rows, lineH);
        while (clip.Step()) {
            for (int row = clip.DisplayStart; row < clip.DisplayEnd; ++row) {
                const std::size_t base = static_cast<std::size_t>(row) * kCols;
                const float y = ImGui::GetCursorPosY();
                ImGui::TextColored(ImVec4(0.55f, 0.55f, 0.6f, 1), "%0*X", addrDigits,
                                   static_cast<unsigned>(info.displayBase + base));
                char ascii[kCols + 1] = {};
                for (int c = 0; c < kCols; ++c) {
                    const std::size_t off = base + static_cast<std::size_t>(c);
                    if (off >= size) {
                        ascii[c] = 0;
                        break;
                    }
                    const std::uint8_t b = debug::readRegion(*m_, region, off).value_or(0);
                    ascii[c] = b >= 0x20 && b < 0x7F ? static_cast<char>(b) : '.';
                    ImGui::SetCursorPos(ImVec2(hexX + charW * 3 * static_cast<float>(c), y));
                    if (static_cast<long long>(off) == v.editOffset) {
                        ImGui::SetNextItemWidth(charW * 2.5f);
                        if (v.focusEdit) {
                            ImGui::SetKeyboardFocusHere();
                            v.focusEdit = false;
                        }
                        ImGui::PushID(static_cast<int>(off));
                        if (ImGui::InputText("##e", v.editBuf.data(), 3,
                                             ImGuiInputTextFlags_CharsHexadecimal |
                                                 ImGuiInputTextFlags_EnterReturnsTrue |
                                                 ImGuiInputTextFlags_AutoSelectAll)) {
                            if (const auto nv = parseHex(v.editBuf.data())) {
                                debug::writeRegion(*m_, region, off, static_cast<std::uint8_t>(*nv), patches_);
                            }
                            v.editOffset = off + 1 < size ? static_cast<long long>(off + 1) : -1;
                            if (v.editOffset >= 0) {
                                std::snprintf(v.editBuf.data(), v.editBuf.size(), "%02X",
                                              debug::readRegion(*m_, region, off + 1).value_or(0));
                                v.focusEdit = true;
                            }
                        } else if (ImGui::IsItemDeactivated() && !v.focusEdit) {
                            v.editOffset = -1;
                        }
                        ImGui::PopID();
                        continue;
                    }
                    const bool changedByte = v.highlight && !running && off < prev.size() && prev[off] != b;
                    const bool hit = std::binary_search(v.hits.begin(), v.hits.end(), off);
                    ImVec4 colour = ImGui::GetStyleColorVec4(ImGuiCol_Text);
                    if (b == 0) {
                        colour = ImVec4(0.5f, 0.5f, 0.55f, 1);
                    }
                    if (hit) {
                        colour = ImVec4(0.4f, 1, 0.5f, 1);
                    }
                    if (changedByte) {
                        colour = ImVec4(1, 0.82f, 0.35f, 1);
                    }
                    ImGui::TextColored(colour, "%02X", b);
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("$%0*X = $%02X (%u)%s", addrDigits,
                                          static_cast<unsigned>(info.displayBase + off), b, b,
                                          running ? "" : "\nclick to edit");
                        if (!running && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                            v.editOffset = static_cast<long long>(off);
                            std::snprintf(v.editBuf.data(), v.editBuf.size(), "%02X", b);
                            v.focusEdit = true;
                        }
                    }
                }
                ImGui::SetCursorPos(ImVec2(asciiX, y));
                ImGui::TextUnformatted(ascii);
            }
        }
    }
    ImGui::EndChild();
    ImGui::End();
}

// ---- Breakpoints (M06-03 / M06-04) ----------------------------------------------------------------------

void DebuggerUi::drawBreakpoints(bool running) {
    (void)running;
    if (!showBreakpoints_) {
        return;
    }
    if (!ImGui::Begin(kBreakpointsWindow, &showBreakpoints_)) {
        ImGui::End();
        return;
    }
    // Editor.
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 7);
    if (ImGui::BeginCombo("##kind", kindName(form_.kind))) {
        for (const debug::BpKind k : kKinds) {
            if (ImGui::Selectable(kindName(k), k == form_.kind)) {
                form_.kind = k;
            }
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    int cpu = form_.cpu == CpuId::Z80 ? 1 : 0;
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 4.5f);
    if (ImGui::Combo("##cpu", &cpu, "68000\0Z80\0")) {
        form_.cpu = cpu == 1 ? CpuId::Z80 : CpuId::M68k;
    }
    const bool decimal =
        form_.kind == debug::BpKind::Interrupt || form_.kind == debug::BpKind::Exception ||
        form_.kind == debug::BpKind::Frame || form_.kind == debug::BpKind::VdpReg;
    ImGui::SameLine();
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 5);
    ImGui::InputTextWithHint("##start", decimal ? "from" : "$start", formStart_.data(), formStart_.size());
    ImGui::SameLine();
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 5);
    ImGui::InputTextWithHint("##end", decimal ? "to" : "$end", formEnd_.data(), formEnd_.size());
    if (form_.kind == debug::BpKind::Dma) {
        ImGui::SameLine();
        int t = form_.dmaTarget + 1;
        ImGui::SetNextItemWidth(ImGui::GetFontSize() * 5);
        if (ImGui::Combo("##dma", &t, "any\0VRAM\0CRAM\0VSRAM\0")) {
            form_.dmaTarget = t - 1;
        }
    }
    if (form_.kind == debug::BpKind::Read || form_.kind == debug::BpKind::Write || form_.kind == debug::BpKind::Access) {
        ImGui::SameLine();
        bool byteSize = (form_.sizes & 1) != 0;
        bool wordSize = (form_.sizes & 2) != 0;
        ImGui::Checkbox("byte", &byteSize);
        ImGui::SameLine();
        ImGui::Checkbox("word", &wordSize);
        form_.sizes = static_cast<std::uint8_t>((byteSize ? 1 : 0) | (wordSize ? 2 : 0) | 4);
    }
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##cond", "condition, e.g. d0.w == $10 && byte($FFB000) != 0 (value / addr for watches)",
                             formCondition_.data(), formCondition_.size());
    ImGui::Checkbox("log", &form_.log);
    if (form_.log) {
        ImGui::SameLine();
        ImGui::Checkbox("and stop", &form_.stop);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-1);
        ImGui::InputTextWithHint("##msg", "message: obj {a0:x} routine {byte(a0+$24)}", formMessage_.data(),
                                 formMessage_.size());
    }
    ImGui::Checkbox("temporary", &form_.temporary);
    ImGui::SameLine();
    int ignore = static_cast<int>(form_.ignoreHits);
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 5);
    if (ImGui::InputInt("skip hits", &ignore)) {
        form_.ignoreHits = static_cast<std::uint64_t>(std::max(ignore, 0));
    }
    ImGui::SameLine();
    int tf = static_cast<int>(form_.traceFrames);
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 5);
    if (ImGui::InputInt("trace frames on hit", &tf)) {
        form_.traceFrames = static_cast<std::uint32_t>(std::max(tf, 0));
    }
    const auto parseBound = [&](const char* text) -> std::optional<std::uint32_t> {
        if (text[0] == 0) {
            return std::nullopt;
        }
        if (decimal) {
            char* end = nullptr;
            const unsigned long v = std::strtoul(text, &end, 10);
            return end != text ? std::optional(static_cast<std::uint32_t>(v)) : std::nullopt;
        }
        return parseHex(text);
    };
    const auto submit = [&](bool update) {
        debug::Breakpoint bp = form_;
        const auto start = parseBound(formStart_.data());
        if (!start && form_.kind != debug::BpKind::Dma) {
            formError_ = "start address / number missing";
            return;
        }
        bp.start = start.value_or(0);
        bp.end = parseBound(formEnd_.data()).value_or(form_.kind == debug::BpKind::Dma && !start ? 0xFFFF : bp.start);
        bp.condition = formCondition_.data();
        bp.message = formMessage_.data();
        if (!bp.log) {
            bp.stop = true;
        }
        formError_.clear();
        if (update) {
            bp.id = editing_;
            if (const debug::Breakpoint* old = d_->find(editing_)) {
                bp.hits = old->hits;
                bp.enabled = old->enabled;
            }
            if (!d_->update(bp, formError_)) {
                return;
            }
        } else if (d_->add(bp, formError_) < 0) {
            return;
        }
    };
    if (ImGui::Button("Add")) {
        submit(false);
    }
    if (editing_ >= 0 && d_->find(editing_) != nullptr) {
        ImGui::SameLine();
        if (ImGui::Button(std::format("Update #{}", editing_).c_str())) {
            submit(true);
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Remove all")) {
        d_->clearBreakpoints();
        editing_ = -1;
    }
    if (!formError_.empty()) {
        ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "%s", formError_.c_str());
    }
    ImGui::Separator();
    if (ImGui::BeginTable("bps", 8,
                          ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_ScrollY |
                              ImGuiTableFlags_Resizable)) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("on", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("kind", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("cpu", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("range", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("condition / message");
        ImGui::TableSetupColumn("hits", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableHeadersRow();
        int removeId = -1;
        int toggleId = -1;
        bool toggleTo = false;
        for (const debug::Breakpoint& bp : d_->breakpoints()) {
            ImGui::PushID(bp.id);
            ImGui::TableNextRow();
            if (d_->lastStop().breakpoint == bp.id) {
                ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg1, IM_COL32(120, 90, 30, 120));
            }
            ImGui::TableNextColumn();
            bool on = bp.enabled;
            if (ImGui::Checkbox("##on", &on)) {
                toggleId = bp.id;
                toggleTo = on;
            }
            ImGui::TableNextColumn();
            if (ImGui::Selectable(std::to_string(bp.id).c_str(), editing_ == bp.id,
                                  ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap)) {
                editing_ = bp.id;
                form_ = bp;
                const bool dec = bp.kind == debug::BpKind::Interrupt || bp.kind == debug::BpKind::Exception ||
                                 bp.kind == debug::BpKind::Frame || bp.kind == debug::BpKind::VdpReg;
                setText(formStart_, dec ? std::to_string(bp.start) : std::format("${:X}", bp.start));
                setText(formEnd_, dec ? std::to_string(bp.end) : std::format("${:X}", bp.end));
                setText(formCondition_, bp.condition);
                setText(formMessage_, bp.message);
                if (bp.kind == debug::BpKind::Exec) {
                    gotoDisassembly(bp.cpu, bp.start);
                }
            }
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(kindName(bp.kind));
            if (bp.temporary) {
                ImGui::SameLine();
                ImGui::TextDisabled("temp");
            }
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(bp.cpu == CpuId::Z80 ? "Z80" : "68k");
            ImGui::TableNextColumn();
            const bool dec = bp.kind == debug::BpKind::Interrupt || bp.kind == debug::BpKind::Exception ||
                             bp.kind == debug::BpKind::Frame || bp.kind == debug::BpKind::VdpReg;
            if (dec) {
                ImGui::Text(bp.start == bp.end ? "%u" : "%u-%u", bp.start, bp.end);
            } else {
                ImGui::Text(bp.start == bp.end ? "$%06X" : "$%06X-$%06X", bp.start, bp.end);
            }
            ImGui::TableNextColumn();
            std::string desc = bp.condition;
            if (bp.log) {
                desc += (desc.empty() ? "" : "  ") + std::string("log \"") + bp.message + "\"" +
                        (bp.stop ? " + stop" : "");
            }
            if (bp.traceFrames != 0) {
                desc += std::format("  trace {} frames", bp.traceFrames);
            }
            ImGui::TextUnformatted(desc.c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%llu", static_cast<unsigned long long>(bp.hits));
            ImGui::TableNextColumn();
            if (ImGui::SmallButton("x")) {
                removeId = bp.id;
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
        if (toggleId >= 0) {
            d_->setEnabled(toggleId, toggleTo);
        }
        if (removeId >= 0) {
            d_->remove(removeId);
        }
    }
    ImGui::End();
}

// ---- Trace (M06-09) -------------------------------------------------------------------------------------

void DebuggerUi::drawTrace(bool running) {
    if (!showTrace_) {
        return;
    }
    if (!ImGui::Begin(kTraceWindow, &showTrace_)) {
        ImGui::End();
        return;
    }
    ImGui::SetNextItemWidth(-ImGui::GetFontSize() * 6);
    ImGui::InputText("trace file", tracePath_.data(), tracePath_.size());
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 5);
    ImGui::InputTextWithHint("##lo", "PC from $", tracePcLo_.data(), tracePcLo_.size());
    ImGui::SameLine();
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 5);
    ImGui::InputTextWithHint("##hi", "PC to $", tracePcHi_.data(), tracePcHi_.size());
    ImGui::SameLine();
    ImGui::Checkbox("first execution only", &traceFirst_);
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 6);
    ImGui::InputInt("frames (0 = until stopped)", &traceFrames_);
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 6);
    ImGui::InputInt("max records (0 = no limit)", &traceMax_);
    d_->setTraceOnHitPath(tracePath_.data());
    if (!d_->tracing()) {
        if (ImGui::Button("Begin trace")) {
            debug::TraceOptions o;
            const auto lo = parseHex(tracePcLo_.data());
            const auto hi = parseHex(tracePcHi_.data());
            if (lo || hi) {
                o.pcRange = std::pair(lo.value_or(0), hi.value_or(0xFFFFFF));
            }
            o.firstExecutionOnly = traceFirst_;
            o.frames = static_cast<std::uint64_t>(std::max(traceFrames_, 0));
            o.maxRecords = traceMax_ > 0 ? static_cast<std::uint64_t>(traceMax_) : UINT64_MAX;
            std::string error;
            traceStatus_ = d_->beginTrace(tracePath_.data(), o, error)
                               ? std::format("tracing from frame {}", m_->frame())
                               : error;
        }
    } else {
        if (ImGui::Button("Stop trace")) {
            const std::uint64_t n = d_->traceRecords();
            d_->endTrace();
            traceStatus_ = std::format("{} records written to {}", n, tracePath_.data());
        } else {
            ImGui::SameLine();
            ImGui::Text("%llu records%s", static_cast<unsigned long long>(d_->traceRecords()),
                        running ? "" : " (paused: step or run to record)");
        }
    }
    if (!traceStatus_.empty()) {
        ImGui::TextDisabled("%s", traceStatus_.c_str());
    }
    ImGui::Separator();
    ImGui::TextUnformatted("Ring buffer: the last N instructions of both CPUs");
    bool ring = d_->ringEnabled();
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 6);
    ImGui::InputInt("entries", &ringSize_, 10000);
    ringSize_ = std::clamp(ringSize_, 16, 10000000);
    ImGui::SameLine();
    if (ImGui::Checkbox("capture", &ring)) {
        d_->setRingEnabled(ring, static_cast<std::size_t>(ringSize_));
    }
    ImGui::SetNextItemWidth(-ImGui::GetFontSize() * 6);
    ImGui::InputText("dump file", ringPath_.data(), ringPath_.size());
    ImGui::BeginDisabled(!ring || running);
    if (ImGui::Button("Dump ring")) {
        std::ofstream out(ringPath_.data(), std::ios::binary);
        d_->dumpRing(out);
        traceStatus_ = out ? std::format("ring ({} entries) written to {}", d_->ring().size(), ringPath_.data())
                           : std::string("cannot write ") + ringPath_.data();
    }
    ImGui::EndDisabled();
    if (ring && !running) {
        // The last few entries, newest last.
        const auto entries = d_->ring();
        const std::size_t from = entries.size() > 12 ? entries.size() - 12 : 0;
        for (std::size_t i = from; i < entries.size(); ++i) {
            const debug::RingEntry& e = entries[i];
            if (e.irq > 0) {
                ImGui::Text("%s %06X  interrupt level %d", e.cpu == CpuId::Z80 ? "Z80 " : "68k ", e.pc, e.irq);
            } else {
                const debug::ListingLine l = debug::disassembleAt(*m_, e.cpu, e.pc);
                ImGui::Text("%s %06X  %s", e.cpu == CpuId::Z80 ? "Z80 " : "68k ", e.pc, l.text.c_str());
            }
        }
    }
    ImGui::End();
}

// ---- Call stack (M06-11) --------------------------------------------------------------------------------

void DebuggerUi::drawCallStack(bool running) {
    if (!showCallStack_) {
        return;
    }
    if (!ImGui::Begin(kCallStackWindow, &showCallStack_)) {
        ImGui::End();
        return;
    }
    bool tracking = d_->callStackTracking();
    if (ImGui::Checkbox("Track calls (exact; costs a hook per instruction)", &tracking)) {
        d_->setCallStackTracking(tracking);
    }
    if (running) {
        ImGui::TextDisabled("Pause to see the stack.");
        ImGui::End();
        return;
    }
    if (!tracking) {
        ImGui::TextDisabled("Heuristic stack walk (return addresses after a JSR / BSR)");
    }
    const auto frames = d_->callStack();
    if (ImGui::BeginTable("stack", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV)) {
        ImGui::TableSetupColumn("frame");
        ImGui::TableSetupColumn("entry");
        ImGui::TableSetupColumn("called from");
        ImGui::TableSetupColumn("returns to");
        ImGui::TableHeadersRow();
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextColored(ImVec4(0.6f, 0.8f, 1, 1), "PC");
        ImGui::TableNextColumn();
        ImGui::Text("$%06X", m_->cpu().state().pc);
        for (auto it = frames.rbegin(); it != frames.rend(); ++it) {
            const debug::CallFrame& f = *it;
            ImGui::PushID(static_cast<int>(it - frames.rbegin()));
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            std::string label;
            if (f.kind == debug::CallFrame::Kind::Call) {
                label = f.verified ? "call" : "call?";
            } else {
                label = debug::describeVector(f.vector);
            }
            if (ImGui::Selectable(label.c_str(), false, ImGuiSelectableFlags_SpanAllColumns)) {
                gotoDisassembly(CpuId::M68k, f.callPc);
            }
            ImGui::TableNextColumn();
            if (f.entry != 0) {
                ImGui::Text("$%06X", f.entry);
            } else {
                ImGui::TextDisabled("?");
            }
            ImGui::TableNextColumn();
            ImGui::Text("$%06X", f.callPc);
            ImGui::TableNextColumn();
            ImGui::Text("$%06X", f.returnPc);
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    ImGui::End();
}

// ---- VDP viewers (M06-07) -------------------------------------------------------------------------------

namespace {

const char* const kVdpRegNames[24] = {"Mode 1",        "Mode 2",         "Plane A name",   "Window name",
                                      "Plane B name",  "Sprite table",   "Sprite 128K",    "Background",
                                      "(unused)",      "(unused)",       "H-int counter",  "Mode 3",
                                      "Mode 4",        "H scroll table", "Plane 128K",     "Auto-increment",
                                      "Plane size",    "Window H pos",   "Window V pos",   "DMA length lo",
                                      "DMA length hi", "DMA source lo",  "DMA source mid", "DMA source hi"};

std::string decodeReg(const vdp::VdpState& s, int r) {
    const std::uint8_t v = s.regs[static_cast<std::size_t>(r)];
    switch (r) {
        case 0:
            return std::format("H-int {}, HV latch {}", (v & 0x10) ? "on" : "off", (v & 0x02) ? "on" : "off");
        case 1:
            return std::format("display {}, V-int {}, DMA {}, {} lines", (v & 0x40) ? "on" : "off",
                               (v & 0x20) ? "on" : "off", (v & 0x10) ? "on" : "off", (v & 0x08) ? 240 : 224);
        case 2:
            return std::format("${:04X}", (v & 0x38) << 10);
        case 3:
            return std::format("${:04X}", (v & 0x3E) << 10);
        case 4:
            return std::format("${:04X}", (v & 0x07) << 13);
        case 5:
            return std::format("${:04X}", (v & 0x7F) << 9);
        case 7:
            return std::format("palette {} colour {}", (v >> 4) & 3, v & 15);
        case 10:
            return std::format("every {} lines", v + 1);
        case 11:
            return std::format("V scroll {}, H scroll {}", (v & 0x04) ? "2-cell" : "full",
                               (v & 3) == 0 ? "full" : (v & 3) == 2 ? "cell" : (v & 3) == 3 ? "line" : "invalid");
        case 12:
            return std::format("{}, shadow/highlight {}, interlace {}", (v & 0x81) ? "H40" : "H32",
                               (v & 0x08) ? "on" : "off", (v >> 1) & 3);
        case 13:
            return std::format("${:04X}", (v & 0x3F) << 10);
        case 15:
            return std::format("+{}", v);
        case 16: {
            const auto cells = [](int bits) { return bits == 0 ? 32 : bits == 1 ? 64 : bits == 3 ? 128 : 0; };
            return std::format("{}x{} cells", cells(v & 3), cells((v >> 4) & 3));
        }
        case 17:
            return std::format("{} {} cells", (v & 0x80) ? "right from" : "left to", (v & 0x1F) * 2);
        case 18:
            return std::format("{} {} cells", (v & 0x80) ? "below" : "above", v & 0x1F);
        case 20:
            return std::format("length {} words", s.regs[19] | (v << 8));
        case 23:
            return (v & 0x80) ? ((v & 0x40) ? "copy" : "fill") : "68k -> VDP";
        default:
            return {};
    }
}

struct PlaneGeometry {
    std::uint32_t base = 0;
    int widthCells = 32;
    int heightCells = 32;
};

PlaneGeometry planeGeometry(const vdp::Vdp& v, int which) {
    const auto& r = v.state().regs;
    const auto cells = [](int bits) { return bits == 0 ? 32 : bits == 1 ? 64 : bits == 3 ? 128 : 32; };
    PlaneGeometry g;
    if (which == 2) {
        g.base = static_cast<std::uint32_t>((r[3] & (v.h40() ? 0x3C : 0x3E)) << 10);
        g.widthCells = v.h40() ? 64 : 32;
        g.heightCells = 32;
        return g;
    }
    g.base = which == 0 ? static_cast<std::uint32_t>((r[2] & 0x38) << 10) : static_cast<std::uint32_t>((r[4] & 7) << 13);
    g.widthCells = cells(r[16] & 3);
    g.heightCells = cells((r[16] >> 4) & 3);
    if (g.widthCells * g.heightCells > 4096) {
        g.heightCells = 4096 / g.widthCells;
    }
    return g;
}

void drawTile(const vdp::VdpState& s, int tile, int palette, bool hflip, bool vflip, std::uint32_t* out, int stride,
              bool transparentZero) {
    const std::size_t base = static_cast<std::size_t>(tile & 0x7FF) * 32;
    for (int y = 0; y < 8; ++y) {
        const int sy = vflip ? 7 - y : y;
        for (int x = 0; x < 8; ++x) {
            const int sx = hflip ? 7 - x : x;
            const std::uint8_t byte = s.vram[(base + static_cast<std::size_t>(sy * 4 + sx / 2)) & 0xFFFF];
            const int idx = (sx & 1) == 0 ? byte >> 4 : byte & 15;
            std::uint32_t c = vdp::cramToRgba(s.cram[static_cast<std::size_t>(palette * 16 + idx) & 63]);
            if (idx == 0 && transparentZero) {
                c = 0xFF202020;
            }
            out[y * stride + x] = c;
        }
    }
}

}  // namespace

void DebuggerUi::drawVdpRegisters() {
    if (!showVdpRegs_) {
        return;
    }
    if (!ImGui::Begin(kVdpRegsWindow, &showVdpRegs_)) {
        ImGui::End();
        return;
    }
    vdp::Vdp& v = m_->vdp();
    vdp::LayerMask& layers = v.layers();
    ImGui::TextUnformatted("Layers:");
    ImGui::SameLine();
    ImGui::Checkbox("B", &layers.planeB);
    ImGui::SameLine();
    ImGui::Checkbox("A", &layers.planeA);
    ImGui::SameLine();
    ImGui::Checkbox("window", &layers.window);
    ImGui::SameLine();
    ImGui::Checkbox("sprites", &layers.sprites);
    ImGui::SameLine();
    int prio = static_cast<int>(layers.priority);
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 6);
    if (ImGui::Combo("priority", &prio, "both\0high only\0low only\0")) {
        layers.priority = static_cast<vdp::LayerMask::Priority>(prio);
    }
    ImGui::TextDisabled("Layer toggles only change the picture (and its frame hash), not the emulation.");
    const vdp::VdpState& s = v.state();
    if (ImGui::BeginTable("vdpregs", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV)) {
        ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("register", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("decoded");
        ImGui::TableHeadersRow();
        for (int r = 0; r < vdp::kRegisterCount; ++r) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("%02d", r);
            ImGui::TableNextColumn();
            ImGui::Text("$%02X", s.regs[static_cast<std::size_t>(r)]);
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(kVdpRegNames[r]);
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(decodeReg(s, r).c_str());
        }
        ImGui::EndTable();
    }
    ImGui::Separator();
    ImGui::Text("Status: V-int %s, H-int %s, sprite overflow %s, collision %s, %s frame, %s", s.vintPending ? "pending" : "-",
                s.hintPending ? "pending" : "-", s.spriteOverflow ? "yes" : "no", s.spriteCollision ? "yes" : "no",
                s.oddFrame ? "odd" : "even", s.pal ? "PAL" : "NTSC");
    ImGui::Text("Access: address $%04X, code $%02X%s, FIFO %d, H-int counter %d, line %d", s.address, s.code,
                s.pending ? " (2nd word pending)" : "", s.fifoUsed, s.hintCounter, s.line);
    ImGui::Text("DMA: %s, %u words left, source $%06X", s.dmaActive ? "running" : "idle", s.dmaRemaining, s.dmaSource);
    ImGui::End();
}

void DebuggerUi::drawTiles() {
    if (!showTiles_) {
        return;
    }
    if (!ImGui::Begin(kTilesWindow, &showTiles_)) {
        ImGui::End();
        return;
    }
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 8);
    ImGui::SliderInt("palette", &tilePalette_, 0, 3);
    constexpr int kTilesPerRow = 32;
    constexpr int kRows = 2048 / kTilesPerRow;
    std::vector<std::uint32_t> pixels(static_cast<std::size_t>(kTilesPerRow * 8 * kRows * 8));
    const vdp::VdpState& s = m_->vdp().state();
    for (int t = 0; t < 2048; ++t) {
        const int tx = (t % kTilesPerRow) * 8;
        const int ty = (t / kTilesPerRow) * 8;
        drawTile(s, t, tilePalette_, false, false, pixels.data() + ty * kTilesPerRow * 8 + tx, kTilesPerRow * 8, false);
    }
    tiles_.update(kTilesPerRow * 8, kRows * 8, pixels.data());
    const float scale = std::max(1.0f, std::floor(ImGui::GetContentRegionAvail().x / (kTilesPerRow * 8)));
    if (ImGui::BeginChild("sheet", ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar)) {
        const ImVec2 origin = ImGui::GetCursorScreenPos();
        ImGui::Image(static_cast<ImTextureID>(tiles_.id()), ImVec2(kTilesPerRow * 8 * scale, kRows * 8 * scale));
        if (ImGui::IsItemHovered()) {
            const ImVec2 mouse = ImGui::GetIO().MousePos;
            const int cx = static_cast<int>((mouse.x - origin.x) / (8 * scale));
            const int cy = static_cast<int>((mouse.y - origin.y) / (8 * scale));
            const int tile = cy * kTilesPerRow + cx;
            if (cx >= 0 && cx < kTilesPerRow && tile >= 0 && tile < 2048) {
                ImGui::BeginTooltip();
                ImGui::Text("tile %d ($%03X)  VRAM $%04X", tile, tile, tile * 32);
                std::array<std::uint32_t, 64> one{};
                drawTile(s, tile, tilePalette_, false, false, one.data(), 8, false);
                // Tooltip zoom drawn as rectangles (no extra texture needed).
                ImDrawList* dl = ImGui::GetWindowDrawList();
                const ImVec2 at = ImGui::GetCursorScreenPos();
                const float z = 10;
                for (int y = 0; y < 8; ++y) {
                    for (int x = 0; x < 8; ++x) {
                        const std::uint32_t c = one[static_cast<std::size_t>(y * 8 + x)];
                        dl->AddRectFilled(ImVec2(at.x + x * z, at.y + y * z), ImVec2(at.x + (x + 1) * z, at.y + (y + 1) * z),
                                          c);
                    }
                }
                ImGui::Dummy(ImVec2(8 * z, 8 * z));
                ImGui::EndTooltip();
            }
        }
    }
    ImGui::EndChild();
    ImGui::End();
}

void DebuggerUi::drawPlanes() {
    if (!showPlanes_) {
        return;
    }
    if (!ImGui::Begin(kPlanesWindow, &showPlanes_)) {
        ImGui::End();
        return;
    }
    ImGui::RadioButton("Plane A", &planeSel_, 0);
    ImGui::SameLine();
    ImGui::RadioButton("Plane B", &planeSel_, 1);
    ImGui::SameLine();
    ImGui::RadioButton("Window", &planeSel_, 2);
    ImGui::SameLine();
    ImGui::Checkbox("viewport", &planeViewport_);
    const vdp::Vdp& v = m_->vdp();
    const vdp::VdpState& s = v.state();
    const PlaneGeometry g = planeGeometry(v, planeSel_);
    const int w = g.widthCells * 8;
    const int h = g.heightCells * 8;
    std::vector<std::uint32_t> pixels(static_cast<std::size_t>(w * h));
    for (int cy = 0; cy < g.heightCells; ++cy) {
        for (int cx = 0; cx < g.widthCells; ++cx) {
            const std::size_t at = (g.base + static_cast<std::uint32_t>((cy * g.widthCells + cx) * 2)) & 0xFFFE;
            const auto entry = static_cast<std::uint16_t>((s.vram[at] << 8) | s.vram[at + 1]);
            drawTile(s, entry & 0x7FF, (entry >> 13) & 3, (entry & 0x0800) != 0, (entry & 0x1000) != 0,
                     pixels.data() + cy * 8 * w + cx * 8, w, false);
        }
    }
    plane_.update(w, h, pixels.data());
    ImGui::Text("nametable $%04X, %dx%d cells", g.base, g.widthCells, g.heightCells);
    const float scale = std::max(1.0f, std::floor(ImGui::GetContentRegionAvail().x / static_cast<float>(w)));
    if (ImGui::BeginChild("plane", ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar)) {
        const ImVec2 origin = ImGui::GetCursorScreenPos();
        ImGui::Image(static_cast<ImTextureID>(plane_.id()), ImVec2(static_cast<float>(w) * scale, static_cast<float>(h) * scale));
        if (ImGui::IsItemHovered()) {
            const ImVec2 mouse = ImGui::GetIO().MousePos;
            const int cx = static_cast<int>((mouse.x - origin.x) / (8 * scale));
            const int cy = static_cast<int>((mouse.y - origin.y) / (8 * scale));
            if (cx >= 0 && cx < g.widthCells && cy >= 0 && cy < g.heightCells) {
                const std::size_t at = (g.base + static_cast<std::uint32_t>((cy * g.widthCells + cx) * 2)) & 0xFFFE;
                const auto entry = static_cast<std::uint16_t>((s.vram[at] << 8) | s.vram[at + 1]);
                ImGui::SetTooltip("cell %d,%d  entry $%04X at VRAM $%04X\ntile %d pal %d%s%s%s", cx, cy, entry,
                                  static_cast<unsigned>(at), entry & 0x7FF, (entry >> 13) & 3,
                                  (entry & 0x8000) ? " high" : "", (entry & 0x0800) ? " hflip" : "",
                                  (entry & 0x1000) ? " vflip" : "");
            }
        }
        if (planeViewport_ && planeSel_ < 2) {
            // Scroll viewport of the first line: H scroll moves the plane right, V scroll down.
            const vdp::LineInfo& li = v.lineInfo()[0];
            const int hs = planeSel_ == 0 ? li.hscrollA : li.hscrollB;
            const int vs = planeSel_ == 0 ? li.vscrollA : li.vscrollB;
            const int x0 = ((-hs) % w + w) % w;
            const int y0 = (vs % h + h) % h;
            const int sw = v.frameWidth();
            const int sh = v.activeHeight();
            ImDrawList* dl = ImGui::GetWindowDrawList();
            // Up to four rectangles where the viewport wraps around the plane.
            for (int dx = 0; dx < 2; ++dx) {
                for (int dy = 0; dy < 2; ++dy) {
                    const float rx = static_cast<float>(x0 - dx * w);
                    const float ry = static_cast<float>(y0 - dy * h);
                    if (rx + static_cast<float>(sw) <= 0 || ry + static_cast<float>(sh) <= 0) {
                        continue;
                    }
                    dl->AddRect(ImVec2(origin.x + rx * scale, origin.y + ry * scale),
                                ImVec2(origin.x + (rx + static_cast<float>(sw)) * scale, origin.y + (ry + static_cast<float>(sh)) * scale),
                                IM_COL32(255, 255, 0, 255), 0, 0, 2.0f);
                }
            }
        }
    }
    ImGui::EndChild();
    ImGui::End();
}

void DebuggerUi::drawSprites() {
    boxes_.clear();
    const auto sprites = m_->vdp().decodeSprites();
    if (spriteBoxes_) {
        for (const vdp::SpriteInfo& sp : sprites) {
            boxes_.push_back(ScreenBox{sp.x, sp.y, sp.widthCells * 8, sp.heightCells * 8,
                                       sp.priority ? IM_COL32(255, 80, 80, 255) : IM_COL32(80, 255, 120, 255)});
        }
    }
    if (!showSprites_) {
        return;
    }
    if (!ImGui::Begin(kSpritesWindow, &showSprites_)) {
        ImGui::End();
        return;
    }
    ImGui::Checkbox("Boxes on the game view (red = high priority)", &spriteBoxes_);
    ImGui::Text("%zu sprites in link order from sprite 0; table at $%04X", sprites.size(),
                (m_->vdp().state().regs[5] & 0x7F) << 9);
    if (ImGui::BeginTable("sprites", 9, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_ScrollY)) {
        ImGui::TableSetupScrollFreeze(0, 1);
        for (const char* c : {"#", "x", "y", "size", "tile", "pal", "prio", "flip", "link"}) {
            ImGui::TableSetupColumn(c);
        }
        ImGui::TableHeadersRow();
        for (const vdp::SpriteInfo& sp : sprites) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("%d", sp.index);
            ImGui::TableNextColumn();
            ImGui::Text("%d", sp.x);
            ImGui::TableNextColumn();
            ImGui::Text("%d", sp.y);
            ImGui::TableNextColumn();
            ImGui::Text("%dx%d", sp.widthCells, sp.heightCells);
            ImGui::TableNextColumn();
            ImGui::Text("$%03X", sp.tile);
            ImGui::TableNextColumn();
            ImGui::Text("%d", sp.palette);
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(sp.priority ? "high" : "low");
            ImGui::TableNextColumn();
            ImGui::Text("%s%s", sp.hflip ? "H" : "", sp.vflip ? "V" : "");
            ImGui::TableNextColumn();
            ImGui::Text("%d", sp.link);
        }
        ImGui::EndTable();
    }
    ImGui::End();
}

void DebuggerUi::drawPalette() {
    if (!showPalette_) {
        return;
    }
    if (!ImGui::Begin(kPaletteWindow, &showPalette_)) {
        ImGui::End();
        return;
    }
    const vdp::VdpState& s = m_->vdp().state();
    const float sz = ImGui::GetFontSize() * 1.4f;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    for (int p = 0; p < 4; ++p) {
        ImGui::Text("%d", p);
        ImGui::SameLine();
        for (int i = 0; i < 16; ++i) {
            const std::uint16_t w = s.cram[static_cast<std::size_t>(p * 16 + i)];
            const ImVec2 at = ImGui::GetCursorScreenPos();
            dl->AddRectFilled(at, ImVec2(at.x + sz, at.y + sz), vdp::cramToRgba(w));
            dl->AddRect(at, ImVec2(at.x + sz, at.y + sz), IM_COL32(0, 0, 0, 255));
            ImGui::PushID(p * 16 + i);
            ImGui::InvisibleButton("c", ImVec2(sz, sz));
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("palette %d colour %d (CRAM $%02X) = $%03X\nR %d G %d B %d", p, i, (p * 16 + i) * 2, w,
                                  (w >> 1) & 7, (w >> 5) & 7, (w >> 9) & 7);
            }
            ImGui::PopID();
            ImGui::SameLine(0, 2);
        }
        ImGui::NewLine();
    }
    ImGui::Separator();
    ImGui::TextUnformatted("CRAM writes during the last frame, per line:");
    const auto& lines = m_->vdp().lineInfo();
    int shown = 0;
    std::string text;
    for (int y = 0; y < m_->vdp().activeHeight() && y < static_cast<int>(lines.size()); ++y) {
        if (lines[static_cast<std::size_t>(y)].cramWrites != 0) {
            text += std::format("{}line {}: {}", shown % 6 == 0 ? (shown == 0 ? "" : "\n") : "   ", y,
                                lines[static_cast<std::size_t>(y)].cramWrites);
            ++shown;
        }
    }
    ImGui::TextUnformatted(shown == 0 ? "none (the palette is constant over the frame)" : text.c_str());
    ImGui::End();
}

}  // namespace gen::app
