#pragma once

#include <filesystem>
#include <string>

struct SDL_Window;

namespace gen::app {

struct WindowOptions {
    const char* title = "genesisEmu";
    int logicalWidth = 1280;  // scaled by the display content scale on creation
    int logicalHeight = 800;
    bool maximized = false;
    bool vsync = true;
};

// SDL3 window with an OpenGL 3.3 core context and the Dear ImGui SDL3/OpenGL3 backends. Owns the ImGui
// context. Hi-DPI aware: sizes and fonts are scaled by the display content scale.
class Window {
public:
    Window() = default;
    ~Window();
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    // Returns false and fills `error` if SDL, the window or the GL context cannot be created.
    bool create(const WindowOptions& options, std::string& error);

    void beginFrame();
    // Renders and presents. With `screenshot`, the frame is also saved as PNG; returns false if that fails.
    bool endFrame(const std::filesystem::path* screenshot = nullptr);

    SDL_Window* handle() const { return window_; }
    float contentScale() const { return contentScale_; }
    bool minimized() const;

    // Size in logical (unscaled) pixels, for persisting to the config.
    void logicalSize(int& width, int& height, bool& maximized) const;

private:
    void destroy();

    SDL_Window* window_ = nullptr;
    void* glContext_ = nullptr;
    float contentScale_ = 1.0f;
    bool sdlInitialised_ = false;
    bool imguiInitialised_ = false;
};

}  // namespace gen::app
