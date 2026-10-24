#include "common/rom_info.hpp"

#include <imgui.h>

#include "app/panels/panels.hpp"

namespace gen::app {

void drawRomInfo(bool* open, const std::optional<cart::Rom>& rom) {
    if (!*open) {
        return;
    }
    if (!ImGui::Begin(kRomInfoWindow, open)) {
        ImGui::End();
        return;
    }
    if (!rom) {
        ImGui::TextDisabled("No ROM loaded.");
        ImGui::End();
        return;
    }

    for (const auto& warning : rom->warnings) {
        ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.2f, 1.0f), "%s", warning.c_str());
    }

    const ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV |
                                  ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_ScrollY;
    if (ImGui::BeginTable("##header", 2, flags)) {
        ImGui::TableSetupColumn("Field", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
        for (const auto& field : common::describeRom(*rom)) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextDisabled("%s", field.label.c_str());
            ImGui::TableNextColumn();
            ImGui::PushID(field.label.c_str());
            ImGui::TextUnformatted(field.value.c_str());
            if (ImGui::BeginPopupContextItem("copy")) {
                if (ImGui::MenuItem("Copy")) {
                    ImGui::SetClipboardText(field.value.c_str());
                }
                ImGui::EndPopup();
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    ImGui::End();
}

}  // namespace gen::app
