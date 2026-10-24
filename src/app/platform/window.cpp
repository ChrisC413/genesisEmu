#include "app/platform/window.hpp"

#include <cstdint>
#include <cstring>
#include <vector>

#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl3.h>

#include "common/paths.hpp"

namespace gen::app {

namespace {

constexpr const char* kGlslVersion = "#version 150";

}  // namespace

Window::~Window() {
    destroy();
}

bool Window::create(const WindowOptions& options, std::string& error) {
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
        error = std::string("SDL_Init failed: ") + SDL_GetError();
        return false;
    }
    sdlInitialised_ = true;

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
#if defined(__APPLE__)
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
#endif
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

    contentScale_ = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());
    if (contentScale_ <= 0.0f) {
        contentScale_ = 1.0f;
    }

    SDL_WindowFlags flags =
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    if (options.maximized) {
        flags |= SDL_WINDOW_MAXIMIZED;
    }
    window_ = SDL_CreateWindow(
        options.title, static_cast<int>(static_cast<float>(options.logicalWidth) * contentScale_),
        static_cast<int>(static_cast<float>(options.logicalHeight) * contentScale_), flags);
    if (window_ == nullptr) {
        error = std::string("SDL_CreateWindow failed: ") + SDL_GetError();
        return false;
    }

    SDL_GLContext context = SDL_GL_CreateContext(window_);
    if (context == nullptr) {
        error = std::string("Cannot create an OpenGL 3.3 core context: ") + SDL_GetError();
        return false;
    }
    glContext_ = context;
    SDL_GL_MakeCurrent(window_, context);
    SDL_GL_SetSwapInterval(options.vsync ? 1 : 0);
    SDL_SetWindowPosition(window_, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    SDL_ShowWindow(window_);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_DockingEnable;
    io.IniFilename = nullptr;  // the layout is persisted in the JSON config instead

    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(contentScale_);
    style.FontScaleDpi = contentScale_;

    ImGui_ImplSDL3_InitForOpenGL(window_, context);
    ImGui_ImplOpenGL3_Init(kGlslVersion);
    imguiInitialised_ = true;
    return true;
}

void Window::beginFrame() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
}

bool Window::endFrame(const std::filesystem::path* screenshot) {
    ImGui::Render();
    const ImGuiIO& io = ImGui::GetIO();
    const int width = static_cast<int>(io.DisplaySize.x * io.DisplayFramebufferScale.x);
    const int height = static_cast<int>(io.DisplaySize.y * io.DisplayFramebufferScale.y);
    glViewport(0, 0, width, height);
    glClearColor(0.06f, 0.06f, 0.08f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    bool saved = true;
    if (screenshot != nullptr) {
        saved = false;
        if (SDL_Surface* surface = SDL_CreateSurface(width, height, SDL_PIXELFORMAT_RGBA32)) {
            std::vector<std::uint8_t> pixels(static_cast<std::size_t>(width) *
                                             static_cast<std::size_t>(height) * 4);
            glPixelStorei(GL_PACK_ALIGNMENT, 1);
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
            const std::size_t row = static_cast<std::size_t>(width) * 4;
            for (int y = 0; y < height; ++y) {  // GL rows are bottom-up
                std::memcpy(static_cast<std::uint8_t*>(surface->pixels) +
                                static_cast<std::size_t>(y) * surface->pitch,
                            pixels.data() + static_cast<std::size_t>(height - 1 - y) * row, row);
            }
            saved = SDL_SavePNG(surface, common::toUtf8(*screenshot).c_str());
            SDL_DestroySurface(surface);
        }
    }
    SDL_GL_SwapWindow(window_);
    return saved;
}

bool Window::minimized() const {
    return (SDL_GetWindowFlags(window_) & SDL_WINDOW_MINIMIZED) != 0;
}

void Window::logicalSize(int& width, int& height, bool& maximized) const {
    maximized = (SDL_GetWindowFlags(window_) & SDL_WINDOW_MAXIMIZED) != 0;
    if (maximized) {
        return;  // keep the restored size from the config
    }
    int w = 0;
    int h = 0;
    SDL_GetWindowSize(window_, &w, &h);
    width = static_cast<int>(static_cast<float>(w) / contentScale_);
    height = static_cast<int>(static_cast<float>(h) / contentScale_);
}

void Window::destroy() {
    if (imguiInitialised_) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();
        imguiInitialised_ = false;
    }
    if (glContext_ != nullptr) {
        SDL_GL_DestroyContext(static_cast<SDL_GLContext>(glContext_));
        glContext_ = nullptr;
    }
    if (window_ != nullptr) {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }
    if (sdlInitialised_) {
        SDL_Quit();
        sdlInitialised_ = false;
    }
}

}  // namespace gen::app
