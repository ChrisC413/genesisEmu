#include "app/platform/file_dialog.hpp"

#include <utility>

#include <SDL3/SDL.h>

#include "common/logging.hpp"
#include "common/paths.hpp"

namespace gen::app {

namespace {

constexpr SDL_DialogFileFilter kRomFilters[] = {
    {"Genesis / Mega Drive ROMs", "bin;md;gen;smd"},
    {"All files", "*"},
};
constexpr SDL_DialogFileFilter kStateFilters[] = {
    {"genesisEmu save states", "gst"},
    {"All files", "*"},
};

}  // namespace

RomFileDialog::RomFileDialog(Kind kind) : kind_(kind), shared_(std::make_shared<Shared>()) {}

void RomFileDialog::open(SDL_Window* parent, const std::optional<std::filesystem::path>& startDir,
                         bool save) {
    {
        std::lock_guard lock(shared_->mutex);
        if (shared_->open) {
            return;
        }
        shared_->open = true;
    }
    startLocation_ = startDir ? common::toUtf8(*startDir) : std::string();
    // The callback receives a heap-allocated reference so the shared state outlives this object if needed.
    auto* ref = new std::shared_ptr<Shared>(shared_);
    const SDL_DialogFileFilter* filters = kind_ == Kind::State ? kStateFilters : kRomFilters;
    const int count = kind_ == Kind::State ? static_cast<int>(SDL_arraysize(kStateFilters))
                                           : static_cast<int>(SDL_arraysize(kRomFilters));
    const char* location = startLocation_.empty() ? nullptr : startLocation_.c_str();
    if (save) {
        SDL_ShowSaveFileDialog(&RomFileDialog::onComplete, ref, parent, filters, count, location);
    } else {
        SDL_ShowOpenFileDialog(&RomFileDialog::onComplete, ref, parent, filters, count, location, false);
    }
}

bool RomFileDialog::isOpen() const {
    std::lock_guard lock(shared_->mutex);
    return shared_->open;
}

std::optional<std::filesystem::path> RomFileDialog::poll() {
    std::lock_guard lock(shared_->mutex);
    return std::exchange(shared_->result, std::nullopt);
}

void RomFileDialog::onComplete(void* userdata, const char* const* files, int /*filter*/) {
    std::unique_ptr<std::shared_ptr<Shared>> ref(static_cast<std::shared_ptr<Shared>*>(userdata));
    Shared& shared = **ref;
    std::lock_guard lock(shared.mutex);
    shared.open = false;
    if (files == nullptr) {
        log::get(log::Subsystem::App).error("File dialog failed: {}", SDL_GetError());
        return;
    }
    if (files[0] != nullptr) {
        shared.result = common::fromUtf8(files[0]);
    }
}

}  // namespace gen::app
