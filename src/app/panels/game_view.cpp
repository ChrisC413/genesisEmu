#include <algorithm>

#include <imgui.h>

#include "app/panels/panels.hpp"

namespace gen::app {

void drawGameView(bool* open, const std::optional<cart::Rom>& rom) {
    if (!*open) {
        return;
    }
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    const bool visible = ImGui::Begin(kGameViewWindow, open);
    ImGui::PopStyleVar();
    if (!visible) {
        ImGui::End();
        return;
    }

    // Letterbox a 4:3 screen into the available area.
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const float w = std::min(avail.x, avail.y * 4.0f / 3.0f);
    const float h = w * 3.0f / 4.0f;
    const ImVec2 tl(origin.x + (avail.x - w) * 0.5f, origin.y + (avail.y - h) * 0.5f);
    const ImVec2 br(tl.x + w, tl.y + h);

    ImDrawList* draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(origin, ImVec2(origin.x + avail.x, origin.y + avail.y), IM_COL32(20, 20, 24, 255));
    draw->AddRectFilled(tl, br, IM_COL32(0, 0, 0, 255));

    const char* line1 =
        rom ? (rom->known ? rom->known->title.data() : rom->header.overseasTitle.c_str()) : "No ROM loaded";
    const char* line2 = rom ? "Emulation starts in M02" : "File > Open ROM...  (Ctrl+O)";
    const float lineH = ImGui::GetTextLineHeightWithSpacing();
    const auto centred = [&](const char* text, float y, ImU32 colour) {
        const ImVec2 size = ImGui::CalcTextSize(text);
        draw->AddText(ImVec2(tl.x + (w - size.x) * 0.5f, y), colour, text);
    };
    const float midY = tl.y + h * 0.5f - lineH;
    centred(line1, midY, IM_COL32(230, 230, 230, 255));
    centred(line2, midY + lineH, IM_COL32(140, 140, 150, 255));

    ImGui::Dummy(avail);
    ImGui::End();
}

}  // namespace gen::app
