// "Sound" panel (spec 06 A-1, A-2, A-4; M05-05): output volume and mix, per-channel mute / solo, the
// sound-chip register write log with CSV export, and WAV recording.

#include <imgui.h>

#include <chrono>
#include <cstdio>
#include <format>
#include <fstream>

#include "app/app.hpp"
#include "common/logging.hpp"

namespace gen::app {

namespace {

std::string timestamp() {
    const auto now = std::chrono::system_clock::now();
    const auto secs = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    return std::to_string(secs);
}

}  // namespace

unsigned App::channelMask() const {
    unsigned solo = 0;
    unsigned muted = 0;
    for (int i = 0; i < audio::kChannelCount; ++i) {
        if (solo_[static_cast<std::size_t>(i)]) {
            solo |= 1u << i;
        }
        if (mute_[static_cast<std::size_t>(i)]) {
            muted |= 1u << i;
        }
    }
    return solo != 0 ? solo : (audio::kChAll & ~muted);
}

void App::toggleWavRecording() {
    if (wav_.isOpen()) {
        wav_.close();
        log::get(log::Subsystem::Audio).info("WAV recording stopped: {}", common::toUtf8(wavPath_));
        return;
    }
    if (!machine_) {
        return;
    }
    wavPath_ = paths_.romDataDir(machineSha1_) / ("recording-" + timestamp() + ".wav");
    std::error_code ec;
    std::filesystem::create_directories(wavPath_.parent_path(), ec);
    if (wav_.open(wavPath_, machine_->sound().sampleRate())) {
        log::get(log::Subsystem::Audio).info("WAV recording to {}", common::toUtf8(wavPath_));
    } else {
        pendingError_ = "Cannot write " + common::toUtf8(wavPath_);
    }
}

void App::drawSoundPanel() {
    if (!showSound_) {
        return;
    }
    if (!ImGui::Begin(kSoundWindow, &showSound_)) {
        ImGui::End();
        return;
    }
    if (!audio_.isOpen()) {
        ImGui::TextColored(ImVec4(1, 0.6f, 0.3f, 1), "No audio device (emulation continues silently)");
    } else {
        ImGui::Text("Queue %d ms, rate adjust %+.3f%%", audio_.queuedMs(), (audio_.ratioAdjust() - 1.0) * 100.0);
    }
    if (ImGui::SliderFloat("Volume", &volume_, 0.0f, 2.0f, "%.2f")) {
        audio_.setVolume(volume_);
    }
    if (machine_) {
        auto& mix = machine_->sound().mix();
        ImGui::SliderFloat("FM level", &mix.fmGain, 0.0f, 2.0f, "%.2f");
        ImGui::SliderFloat("PSG level", &mix.psgGain, 0.0f, 2.0f, "%.2f");
        ImGui::Checkbox("Model 1 low-pass (3.4 kHz)", &mix.lowPass);
    }

    ImGui::SeparatorText("Channels");
    if (ImGui::BeginTable("channels", 3, ImGuiTableFlags_SizingFixedFit)) {
        for (int i = 0; i < audio::kChannelCount; ++i) {
            ImGui::PushID(i);
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(audio::channelName(i));
            ImGui::TableNextColumn();
            ImGui::Checkbox("Mute", &mute_[static_cast<std::size_t>(i)]);
            ImGui::TableNextColumn();
            ImGui::Checkbox("Solo", &solo_[static_cast<std::size_t>(i)]);
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    if (ImGui::Button("Clear mute / solo")) {
        mute_.fill(false);
        solo_.fill(false);
    }

    ImGui::SeparatorText("Recording");
    if (ImGui::Button(wav_.isOpen() ? "Stop WAV recording" : "Record WAV", ImVec2(160, 0))) {
        toggleWavRecording();
    }
    if (wav_.isOpen()) {
        ImGui::SameLine();
        ImGui::Text("%.1f s -> %s", static_cast<double>(wav_.frames()) / machine_->sound().sampleRate(),
                    common::toUtf8(wavPath_.filename()).c_str());
    }

    ImGui::SeparatorText("Register write log");
    if (machine_) {
        auto& log = machine_->z80().log();
        bool enabled = log.enabled();
        if (ImGui::Checkbox("Log writes", &enabled)) {
            log.setEnabled(enabled);
        }
        ImGui::SameLine();
        ImGui::Text("%zu kept, %llu total", log.size(), static_cast<unsigned long long>(log.total()));
        if (ImGui::Button("Clear")) {
            log.clear();
        }
        ImGui::SameLine();
        if (ImGui::Button("Export CSV")) {
            const auto path = paths_.romDataDir(machineSha1_) / ("reglog-" + timestamp() + ".csv");
            std::error_code ec;
            std::filesystem::create_directories(path.parent_path(), ec);
            std::ofstream out(path, std::ios::binary);
            out << log.toCsv();
            lastExport_ = out ? common::toUtf8(path) : "export failed: " + common::toUtf8(path);
        }
        if (!lastExport_.empty()) {
            ImGui::TextWrapped("%s", lastExport_.c_str());
        }
        constexpr ImGuiTableFlags flags = ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg |
                                          ImGuiTableFlags_BordersOuter | ImGuiTableFlags_SizingFixedFit;
        if (ImGui::BeginTable("reglog", 7, flags, ImVec2(0, 260))) {
            ImGui::TableSetupScrollFreeze(0, 1);
            for (const char* h : {"frame", "line", "cpu", "pc", "chip", "reg", "value"}) {
                ImGui::TableSetupColumn(h);
            }
            ImGui::TableHeadersRow();
            ImGuiListClipper clipper;
            clipper.Begin(static_cast<int>(log.size()));
            while (clipper.Step()) {
                for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
                    // Newest first.
                    const auto& w = log.at(log.size() - 1 - static_cast<std::size_t>(row));
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::Text("%llu", static_cast<unsigned long long>(w.frame));
                    ImGui::TableNextColumn();
                    ImGui::Text("%u", w.line);
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(w.cpu == CpuId::Z80 ? "z80" : "68k");
                    ImGui::TableNextColumn();
                    ImGui::Text("%06X", w.pc);
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(w.chip == audio::Chip::Psg ? "PSG" : (w.port != 0 ? "YM II" : "YM I"));
                    ImGui::TableNextColumn();
                    ImGui::Text("%02X", w.reg);
                    ImGui::TableNextColumn();
                    ImGui::Text("%02X", w.value);
                }
            }
            ImGui::EndTable();
        }
    }
    ImGui::End();
}

}  // namespace gen::app
