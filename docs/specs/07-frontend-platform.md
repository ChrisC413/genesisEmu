# 07 · Frontend and Platform

## Platforms

| Platform | Toolchain | Package |
|----------|-----------|---------|
| Windows 11 x64 | MSVC 2022 (C++20) | Portable folder: `genesisEmu.exe` + DLLs; later an installer |
| macOS 15+ | Apple clang (Xcode 16+) | `.app` bundle; arm64 primary, x86_64 best effort; ad-hoc signed for local use |

(D-015, D-025: deployment target 15.0.) Windows is built and tested in GitHub Actions; macOS is built and tested locally with
`tools/validate-mac.sh` (D-016, spec 13).

CMake presets: `win-debug`, `win-release`, `mac-debug`, `mac-release`. Builds must work from a clean clone
with only CMake, a compiler and vcpkg installed.

## Application shell

- SDL3 window with an OpenGL 3.3 core (or SDL_GPU) backend for ImGui; Hi-DPI aware.
- **Dear ImGui docking** layout: the game view is a dockable panel; all debugger/analysis panels dock around it. Layout persisted per user.
- Main menu: File (Open ROM, Recent, Open/Save project, Save/Load state), Emulation (Run/Pause, Reset, Region), Debug (panels), Analysis (panels), Help.
- Game view: integer scale or fit, 4:3 aspect correction toggle, nearest/linear filtering.
- Native file dialogs via SDL3.
- Fullscreen mode for "just playing".

## Input

- Keyboard and gamepads via SDL3's gamepad API (hot-plug).
- Two controller ports; each configurable as 3-button or 6-button pad.
- Default keyboard map: arrows = D-pad, A/S/D = A/B/C, Q/W/E = X/Y/Z, Enter = Start, Right Shift = Mode.
- Rebinding UI with "press a key" capture; saved to config.
- Hotkeys (configurable): pause (F5 / Space when the game view is unfocused), step instruction (F11), step over (F10), step frame (F6), save state (F1–F4 slots with Shift), load state.
- Input is sampled once per emulated frame (at V-blank) to keep determinism.

## Configuration and paths

- Config: JSON at the platform user-config directory (`%APPDATA%\genesisEmu\` / `~/Library/Application Support/genesisEmu/`).
- User data (SRAM, save states, projects, logs) under the same root, in subfolders keyed by ROM SHA-1.
- API keys (spec 11) are **never** stored in the main config; they come from the `ANTHROPIC_API_KEY` environment variable or the OS keychain (Windows Credential Manager / macOS Keychain).

## Frame pacing

- Target 59.92 Hz (NTSC) / 49.70 Hz (PAL). Video driven by host vsync when the host refresh is ≈ 60 Hz, with audio dynamic rate control absorbing the difference; otherwise timer-paced.
- Fast-forward (hold Tab) runs unthrottled with audio muted.
- Frame-time overlay (optional) showing emulation ms/frame.

## Logging

spdlog to console and a rotating file; per-subsystem levels (cpu, vdp, bus, audio, analysis, llm).
