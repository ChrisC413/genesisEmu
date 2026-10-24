# M04-02 · Keyboard/gamepad mapping and rebinding

**Milestone:** [M04 – Input, Z80, playable](README.md)  
**Status:** Draft  
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
- [ ] Bindings persist across restarts

## Test plan

- Manual test with Xbox and PlayStation controllers
