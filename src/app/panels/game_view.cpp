#include <algorithm>
#include <cmath>

#include <SDL3/SDL_opengl.h>
#include <imgui.h>

#include "app/panels/panels.hpp"
#include "core/vdp/vdp.hpp"

namespace gen::app {

GameView::~GameView() {
    if (texture_ != 0) {
        const auto tex = static_cast<GLuint>(texture_);
        glDeleteTextures(1, &tex);
    }
}

void GameView::upload(const vdp::Vdp& vdp) {
    if (texture_ == 0) {
        GLuint tex = 0;
        glGenTextures(1, &tex);
        texture_ = tex;
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, vdp::kFbWidth, vdp::kFbHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                     nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        linearApplied_ = !settings_.linearFilter;  // force the filter update below
    }
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(texture_));
    if (linearApplied_ != settings_.linearFilter) {
        const GLint filter = settings_.linearFilter ? GL_LINEAR : GL_NEAREST;
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
        linearApplied_ = settings_.linearFilter;
    }
    width_ = vdp.frameWidth();
    height_ = vdp.frameHeight();
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, vdp::kFbWidth, height_, GL_RGBA, GL_UNSIGNED_BYTE,
                    vdp.framebuffer());
    hasFrame_ = true;
    ++uploads_;
}

void GameView::draw(bool* open, const std::optional<cart::Rom>& rom, const std::string& overlay) {
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

    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    ImDrawList* draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(origin, ImVec2(origin.x + avail.x, origin.y + avail.y), IM_COL32(20, 20, 24, 255));

    // Logical picture size: interlace mode 2 frames have twice the lines in the same picture height.
    const float lines = static_cast<float>(height_ > 240 ? height_ / 2 : height_);
    const float baseW = settings_.aspectCorrect ? lines * 4.0f / 3.0f : static_cast<float>(width_);
    const float baseH = lines;
    float scale = std::min(avail.x / baseW, avail.y / baseH);
    if (settings_.scale == GameViewSettings::Scale::Integer) {
        scale = std::max(1.0f, std::floor(scale));
    }
    const float w = baseW * scale;
    const float h = baseH * scale;
    const ImVec2 tl(std::floor(origin.x + (avail.x - w) * 0.5f), std::floor(origin.y + (avail.y - h) * 0.5f));
    const ImVec2 br(tl.x + w, tl.y + h);

    if (hasFrame_ && texture_ != 0) {
        const ImVec2 uv1(static_cast<float>(width_) / vdp::kFbWidth,
                         static_cast<float>(height_) / vdp::kFbHeight);
        draw->AddImage(static_cast<ImTextureID>(texture_), tl, br, ImVec2(0, 0), uv1);
    } else {
        draw->AddRectFilled(tl, br, IM_COL32(0, 0, 0, 255));
        const char* line1 = rom ? (rom->known ? rom->known->title.data() : rom->header.overseasTitle.c_str())
                                : "No ROM loaded";
        const char* line2 = rom ? "" : "File > Open ROM...  (Ctrl+O)";
        const float lineH = ImGui::GetTextLineHeightWithSpacing();
        const auto centred = [&](const char* text, float y, ImU32 colour) {
            const ImVec2 size = ImGui::CalcTextSize(text);
            draw->AddText(ImVec2(tl.x + (w - size.x) * 0.5f, y), colour, text);
        };
        const float midY = tl.y + h * 0.5f - lineH;
        centred(line1, midY, IM_COL32(230, 230, 230, 255));
        centred(line2, midY + lineH, IM_COL32(140, 140, 150, 255));
    }
    if (settings_.showOverlay && !overlay.empty()) {
        const ImVec2 pad(6, 4);
        const ImVec2 size = ImGui::CalcTextSize(overlay.c_str());
        const ImVec2 at(origin.x + pad.x, origin.y + pad.y);
        draw->AddRectFilled(ImVec2(at.x - 3, at.y - 2), ImVec2(at.x + size.x + 3, at.y + size.y + 2),
                            IM_COL32(0, 0, 0, 160));
        draw->AddText(at, IM_COL32(220, 220, 220, 255), overlay.c_str());
    }

    ImGui::Dummy(avail);
    if (ImGui::BeginPopupContextItem("game_view_menu")) {
        if (ImGui::MenuItem("Fit to panel", nullptr, settings_.scale == GameViewSettings::Scale::Fit)) {
            settings_.scale = GameViewSettings::Scale::Fit;
        }
        if (ImGui::MenuItem("Integer scale", nullptr, settings_.scale == GameViewSettings::Scale::Integer)) {
            settings_.scale = GameViewSettings::Scale::Integer;
        }
        ImGui::MenuItem("4:3 aspect correction", nullptr, &settings_.aspectCorrect);
        ImGui::MenuItem("Linear filtering", nullptr, &settings_.linearFilter);
        ImGui::MenuItem("Frame-time overlay", nullptr, &settings_.showOverlay);
        ImGui::EndPopup();
    }
    ImGui::End();
}

}  // namespace gen::app
