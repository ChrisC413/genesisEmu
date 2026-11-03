#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

struct SDL_Window;

namespace gen::app {

// Native open / save dialog (SDL3) for ROMs or save states. SDL may invoke the completion callback on another
// thread, so the result is handed over through a mutex and collected on the main thread with poll().
class RomFileDialog {
public:
    enum class Kind : std::uint8_t { Rom, State };
    explicit RomFileDialog(Kind kind = Kind::Rom);

    // `save`: a "save as" dialog (the chosen file need not exist).
    void open(SDL_Window* parent, const std::optional<std::filesystem::path>& startDir, bool save = false);
    bool isOpen() const;

    // The chosen file, once, after the user confirms the dialog.
    std::optional<std::filesystem::path> poll();

private:
    struct Shared {
        std::mutex mutex;
        bool open = false;
        std::optional<std::filesystem::path> result;
    };
    static void onComplete(void* userdata, const char* const* files, int filter);

    Kind kind_;
    std::shared_ptr<Shared> shared_;
    std::string startLocation_;  // must outlive the dialog call
};

}  // namespace gen::app
