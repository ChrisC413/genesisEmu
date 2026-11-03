# M04-02 · Keyboard/gamepad mapping and rebinding

**Milestone:** [M04 – Input, Z80, playable](README.md)  
**Status:** Implemented; gamepad hot-plug needs a manual check  
**Depends on:** M04-01  
**Spec refs:** [07-frontend-platform](../../docs/specs/07-frontend-platform.md#input)

## Story

As a **player**, I want keyboard and gamepad support with rebinding, so that I can play comfortably.

## Scope

- SDL3 gamepad hot-plug
- Default keymap per spec 07
- Rebinding UI with capture
- Hotkey table (pause, step, save/load state) — handlers stubbed until M06

## Acceptance criteria

- [ ] Plug/unplug a gamepad while running without crash
- [x] Bindings persist across restarts

## Test plan

- Manual test with Xbox and PlayStation controllers

## Notes

- `app::InputManager` (src/app/input.*): spec 07 default keymap, per-port keyboard bindings, one gamepad per
  port (opened / closed on SDL hot-plug events, left stick doubles as the D-pad), pad type per port
  (3-button / 6-button / none), and the **Input** panel (Emulation > Input...) with "press a key" /
  "press a gamepad button" capture for every binding and hotkey. Esc cancels a capture.
- Hotkey table in the config: pause (F5), step frame (F6, while paused), fast-forward (hold Tab), step
  instruction (F11), step over (F10), save/load state (Shift+F1-F4 / F1-F4). Step instruction / step over /
  save states only log "not available yet" until M06.
- Config: `input` (pad types, keys per port, gamepad, hotkeys) and `view` (scale, aspect, filtering, overlay;
  the game-view settings approved as an M03 follow-up) sections; round-trip unit test
  `View settings and input bindings round-trip (M04-02)`.
- **Not verified:** plugging / unplugging a gamepad while running (no controller attached to the build
  machine) and the manual Xbox / PlayStation test; to be checked by Chris.
