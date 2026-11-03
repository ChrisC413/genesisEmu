#include "app/input.hpp"

#include <algorithm>
#include <cstdlib>

#include <imgui.h>

#include "common/logging.hpp"

namespace gen::app {

namespace {

constexpr Sint16 kStickThreshold = 16000;

auto& logger() {
    return log::get(log::Subsystem::App);
}

SDL_Scancode scancodeFromName(const std::string& name) {
    return name.empty() ? SDL_SCANCODE_UNKNOWN : SDL_GetScancodeFromName(name.c_str());
}

}  // namespace

bus::PadType padTypeFromString(std::string_view s) {
    if (s == "6") {
        return bus::PadType::SixButton;
    }
    if (s == "none") {
        return bus::PadType::None;
    }
    return bus::PadType::ThreeButton;
}

InputManager::~InputManager() {
    for (SDL_Gamepad* p : pads_) {
        SDL_CloseGamepad(p);
    }
}

void InputManager::configure(const common::InputConfig& config) {
    cfg_ = config;
    resolve();
}

void InputManager::resolve() {
    for (std::size_t port = 0; port < 2; ++port) {
        for (std::size_t i = 0; i < kPadButtons.size(); ++i) {
            const auto it = cfg_.keys[port].find(kPadButtons[i].name);
            keys_[port][i] =
                it != cfg_.keys[port].end() ? scancodeFromName(it->second) : SDL_SCANCODE_UNKNOWN;
        }
    }
    for (std::size_t i = 0; i < kPadButtons.size(); ++i) {
        const auto it = cfg_.gamepad.find(kPadButtons[i].name);
        padButtons_[i] = it != cfg_.gamepad.end() ? SDL_GetGamepadButtonFromString(it->second.c_str())
                                                  : SDL_GAMEPAD_BUTTON_INVALID;
    }
}

bool InputManager::matchesHotkey(std::string_view binding, SDL_Scancode sc, SDL_Keymod mod) {
    bool shift = false;
    bool ctrl = false;
    bool alt = false;
    for (;;) {
        if (binding.starts_with("Shift+")) {
            shift = true;
            binding.remove_prefix(6);
        } else if (binding.starts_with("Ctrl+")) {
            ctrl = true;
            binding.remove_prefix(5);
        } else if (binding.starts_with("Alt+")) {
            alt = true;
            binding.remove_prefix(4);
        } else {
            break;
        }
    }
    if (shift != ((mod & SDL_KMOD_SHIFT) != 0) || ctrl != ((mod & SDL_KMOD_CTRL) != 0) ||
        alt != ((mod & SDL_KMOD_ALT) != 0)) {
        return false;
    }
    return sc != SDL_SCANCODE_UNKNOWN && SDL_GetScancodeFromName(std::string(binding).c_str()) == sc;
}

void InputManager::handleEvent(const SDL_Event& event, bool keyboardFree) {
    switch (event.type) {
        case SDL_EVENT_GAMEPAD_ADDED:
            if (SDL_Gamepad* p = SDL_OpenGamepad(event.gdevice.which)) {
                pads_.push_back(p);
                logger().info("gamepad connected: {}",
                              SDL_GetGamepadName(p) != nullptr ? SDL_GetGamepadName(p) : "?");
            }
            break;
        case SDL_EVENT_GAMEPAD_REMOVED: {
            const auto it = std::find_if(pads_.begin(), pads_.end(), [&](SDL_Gamepad* p) {
                return SDL_GetGamepadID(p) == event.gdevice.which;
            });
            if (it != pads_.end()) {
                logger().info("gamepad disconnected");
                SDL_CloseGamepad(*it);
                pads_.erase(it);
            }
            break;
        }
        case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
            if (capture_.gamepad && capture_.button >= 0) {
                const char* name =
                    SDL_GetGamepadStringForButton(static_cast<SDL_GamepadButton>(event.gbutton.button));
                if (name != nullptr) {
                    cfg_.gamepad[kPadButtons[static_cast<std::size_t>(capture_.button)].name] = name;
                    changed_ = true;
                }
                capture_ = Capture{};
                resolve();
            }
            break;
        case SDL_EVENT_KEY_DOWN: {
            const SDL_Scancode sc = event.key.scancode;
            if (capture_.active() && !capture_.gamepad) {
                if (sc != SDL_SCANCODE_ESCAPE) {
                    const char* name = SDL_GetScancodeName(sc);
                    if (!capture_.hotkey.empty()) {
                        std::string binding;
                        if ((event.key.mod & SDL_KMOD_CTRL) != 0) {
                            binding += "Ctrl+";
                        }
                        if ((event.key.mod & SDL_KMOD_ALT) != 0) {
                            binding += "Alt+";
                        }
                        if ((event.key.mod & SDL_KMOD_SHIFT) != 0 && sc != SDL_SCANCODE_LSHIFT &&
                            sc != SDL_SCANCODE_RSHIFT) {
                            binding += "Shift+";
                        }
                        cfg_.hotkeys[capture_.hotkey] = binding + name;
                    } else {
                        cfg_.keys[static_cast<std::size_t>(capture_.port)]
                                 [kPadButtons[static_cast<std::size_t>(capture_.button)].name] = name;
                    }
                    changed_ = true;
                }
                capture_ = Capture{};
                resolve();
                break;
            }
            if (!keyboardFree || event.key.repeat) {
                break;
            }
            for (const auto& [action, binding] : cfg_.hotkeys) {
                if (action != "fast-forward" && matchesHotkey(binding, sc, event.key.mod)) {
                    pressed_.push_back(action);
                }
            }
            break;
        }
        default:
            break;
    }
}

std::vector<std::string> InputManager::takeHotkeys() {
    std::vector<std::string> out;
    out.swap(pressed_);
    return out;
}

bool InputManager::hotkeyHeld(std::string_view action, bool keyboardFree) const {
    const auto it = cfg_.hotkeys.find(std::string(action));
    if (!keyboardFree || it == cfg_.hotkeys.end()) {
        return false;
    }
    const SDL_Scancode sc = SDL_GetScancodeFromName(it->second.c_str());
    int count = 0;
    const bool* state = SDL_GetKeyboardState(&count);
    return sc != SDL_SCANCODE_UNKNOWN && static_cast<int>(sc) < count && state[sc];
}

std::uint16_t InputManager::buttons(int port, bool keyboardFree) const {
    const auto p = static_cast<std::size_t>(port & 1);
    std::uint16_t mask = 0;
    if (keyboardFree && !capture_.active()) {
        int count = 0;
        const bool* state = SDL_GetKeyboardState(&count);
        for (std::size_t i = 0; i < kPadButtons.size(); ++i) {
            const SDL_Scancode sc = keys_[p][i];
            if (sc != SDL_SCANCODE_UNKNOWN && static_cast<int>(sc) < count && state[sc]) {
                mask = static_cast<std::uint16_t>(mask | kPadButtons[i].mask);
            }
        }
    }
    if (p < pads_.size() && !capture_.gamepad) {
        SDL_Gamepad* pad = pads_[p];
        for (std::size_t i = 0; i < kPadButtons.size(); ++i) {
            if (padButtons_[i] != SDL_GAMEPAD_BUTTON_INVALID && SDL_GetGamepadButton(pad, padButtons_[i])) {
                mask = static_cast<std::uint16_t>(mask | kPadButtons[i].mask);
            }
        }
        // The left stick also works as the D-pad.
        const Sint16 x = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTX);
        const Sint16 y = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTY);
        if (x < -kStickThreshold) {
            mask |= bus::kPadLeft;
        } else if (x > kStickThreshold) {
            mask |= bus::kPadRight;
        }
        if (y < -kStickThreshold) {
            mask |= bus::kPadUp;
        } else if (y > kStickThreshold) {
            mask |= bus::kPadDown;
        }
    }
    // Opposite directions cancel (a real D-pad cannot press both).
    if ((mask & (bus::kPadLeft | bus::kPadRight)) == (bus::kPadLeft | bus::kPadRight)) {
        mask = static_cast<std::uint16_t>(mask & ~(bus::kPadLeft | bus::kPadRight));
    }
    if ((mask & (bus::kPadUp | bus::kPadDown)) == (bus::kPadUp | bus::kPadDown)) {
        mask = static_cast<std::uint16_t>(mask & ~(bus::kPadUp | bus::kPadDown));
    }
    return mask;
}

std::string InputManager::gamepadName(std::size_t i) const {
    const char* name = i < pads_.size() ? SDL_GetGamepadName(pads_[i]) : nullptr;
    return name != nullptr ? name : "gamepad";
}

bool InputManager::drawPanel(bool* open) {
    if (!*open) {
        return false;
    }
    if (!ImGui::Begin(kInputWindow, open)) {
        ImGui::End();
        return std::exchange(changed_, false);
    }
    static constexpr const char* kTypes[3] = {"3-button", "6-button", "none"};
    static constexpr const char* kTypeValues[3] = {"3", "6", "none"};
    for (std::size_t port = 0; port < 2; ++port) {
        int current = 0;
        for (int i = 0; i < 3; ++i) {
            if (cfg_.padType[port] == kTypeValues[i]) {
                current = i;
            }
        }
        ImGui::SetNextItemWidth(120);
        const std::string label = "Port " + std::to_string(port + 1);
        if (ImGui::Combo(label.c_str(), &current, kTypes, 3)) {
            cfg_.padType[port] = kTypeValues[current];
            changed_ = true;
        }
        ImGui::SameLine();
    }
    ImGui::NewLine();
    if (pads_.empty()) {
        ImGui::TextDisabled("No gamepad connected");
    }
    for (std::size_t i = 0; i < pads_.size(); ++i) {
        ImGui::Text("Gamepad %zu (port %zu): %s", i + 1, i + 1, gamepadName(i).c_str());
    }
    if (capture_.active()) {
        ImGui::TextColored(ImVec4(1, 0.8f, 0.2f, 1),
                           capture_.gamepad ? "Press a gamepad button..." : "Press a key... (Esc cancels)");
    }
    const auto cell = [&](const std::string& text, bool capturing, const char* id) {
        ImGui::PushID(id);
        const bool clicked =
            ImGui::Button(capturing ? "..." : (text.empty() ? "-" : text.c_str()), ImVec2(-1, 0));
        ImGui::PopID();
        return clicked;
    };
    if (ImGui::BeginTable("bindings", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Button");
        ImGui::TableSetupColumn("Port 1 key");
        ImGui::TableSetupColumn("Port 2 key");
        ImGui::TableSetupColumn("Gamepad");
        ImGui::TableHeadersRow();
        for (std::size_t i = 0; i < kPadButtons.size(); ++i) {
            const std::string name = kPadButtons[i].name;
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(name.c_str());
            for (int port = 0; port < 2; ++port) {
                ImGui::TableNextColumn();
                const auto& keys = cfg_.keys[static_cast<std::size_t>(port)];
                const auto it = keys.find(name);
                const bool capturing = capture_.port == port && capture_.button == static_cast<int>(i);
                if (cell(it != keys.end() ? it->second : "", capturing,
                         (name + std::to_string(port)).c_str())) {
                    capture_ = Capture{};
                    capture_.port = port;
                    capture_.button = static_cast<int>(i);
                }
            }
            ImGui::TableNextColumn();
            const auto it = cfg_.gamepad.find(name);
            const bool capturing = capture_.gamepad && capture_.button == static_cast<int>(i);
            if (cell(it != cfg_.gamepad.end() ? it->second : "", capturing, (name + "pad").c_str())) {
                capture_ = Capture{};
                capture_.gamepad = true;
                capture_.button = static_cast<int>(i);
            }
        }
        ImGui::EndTable();
    }
    if (ImGui::CollapsingHeader("Hotkeys")) {
        ImGui::TextDisabled("Step actions take effect with the debugger (M06).");
        if (ImGui::BeginTable("hotkeys", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
            for (const auto& [action, binding] : cfg_.hotkeys) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(action.c_str());
                ImGui::TableNextColumn();
                if (cell(binding, capture_.hotkey == action, action.c_str())) {
                    capture_ = Capture{};
                    capture_.hotkey = action;
                }
            }
            ImGui::EndTable();
        }
    }
    if (ImGui::Button("Reset to defaults")) {
        cfg_ = common::defaultInputConfig();
        capture_ = Capture{};
        changed_ = true;
    }
    ImGui::End();
    resolve();
    return std::exchange(changed_, false);
}

}  // namespace gen::app
