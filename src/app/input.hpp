#pragma once
// Keyboard / gamepad mapping, rebinding and hotkeys (spec 07 "Input", M04-02). SDL3 gamepads are opened on
// hot-plug; the first one drives port 1, the second port 2. The panel rebinds with "press a key" capture.

#include <array>
#include <cstdint>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include <SDL3/SDL.h>

#include "common/config.hpp"
#include "core/bus/io.hpp"

namespace gen::app {

inline constexpr const char* kInputWindow = "Input";

// Pad button names in config order, with their bus::PadButton masks.
struct PadButtonName {
    const char* name;
    std::uint16_t mask;
};
inline constexpr std::array<PadButtonName, 12> kPadButtons{{{"UP", bus::kPadUp},
                                                            {"DOWN", bus::kPadDown},
                                                            {"LEFT", bus::kPadLeft},
                                                            {"RIGHT", bus::kPadRight},
                                                            {"A", bus::kPadA},
                                                            {"B", bus::kPadB},
                                                            {"C", bus::kPadC},
                                                            {"START", bus::kPadStart},
                                                            {"X", bus::kPadX},
                                                            {"Y", bus::kPadY},
                                                            {"Z", bus::kPadZ},
                                                            {"MODE", bus::kPadMode}}};

bus::PadType padTypeFromString(std::string_view s);

class InputManager {
public:
    InputManager() = default;
    ~InputManager();
    InputManager(const InputManager&) = delete;
    InputManager& operator=(const InputManager&) = delete;

    void configure(const common::InputConfig& config);
    const common::InputConfig& config() const { return cfg_; }

    // Gamepad hot-plug, rebinding capture and hotkey presses. `keyboardFree` is false while a text field
    // has focus (keys then neither reach the game nor fire hotkeys).
    void handleEvent(const SDL_Event& event, bool keyboardFree);
    // Hotkey presses since the last call (action names, e.g. "pause", "save-state-1").
    std::vector<std::string> takeHotkeys();
    // Whether the key bound to `action` is held (fast-forward).
    bool hotkeyHeld(std::string_view action, bool keyboardFree) const;

    // Pressed buttons of `port` (0/1) from its keyboard bindings and its gamepad.
    std::uint16_t buttons(int port, bool keyboardFree) const;
    bus::PadType padType(int port) const {
        return padTypeFromString(cfg_.padType[static_cast<std::size_t>(port & 1)]);
    }

    std::size_t gamepadCount() const { return pads_.size(); }
    std::string gamepadName(std::size_t i) const;

    // The "Input" panel. Returns true when the configuration changed (save it).
    bool drawPanel(bool* open);

private:
    struct Capture {
        int port = -1;  // keyboard binding of pad button `button` on `port`
        int button = -1;
        bool gamepad = false;  // gamepad binding of pad button `button`
        std::string hotkey;    // hotkey action
        bool active() const { return port >= 0 || gamepad || !hotkey.empty(); }
    };

    void resolve();
    static bool matchesHotkey(std::string_view binding, SDL_Scancode sc, SDL_Keymod mod);

    common::InputConfig cfg_ = common::defaultInputConfig();
    std::array<std::array<SDL_Scancode, kPadButtons.size()>, 2> keys_{};
    std::array<SDL_GamepadButton, kPadButtons.size()> padButtons_{};
    std::vector<SDL_Gamepad*> pads_;
    std::vector<std::string> pressed_;
    Capture capture_;
    bool changed_ = false;
};

}  // namespace gen::app
