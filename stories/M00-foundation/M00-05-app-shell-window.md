# M00-05 · Application shell: SDL3 window with ImGui docking

**Milestone:** [M00 – Foundation](README.md)  
**Status:** Draft  
**Depends on:** M00-01, M00-03, M00-04  
**Spec refs:** [07-frontend-platform](../../docs/specs/07-frontend-platform.md#application-shell)

## Story

As a **reverse engineer**, I want a desktop window with a docking layout, menus and an empty game view, so that every later panel has a home.

## Scope

- SDL3 window + renderer backend for ImGui (OpenGL 3.3 or SDL_GPU), Hi-DPI
- Docking layout persisted to config
- Main menu skeleton (File/Emulation/Debug/Analysis/Help)
- File → Open ROM via native dialog, Recent ROMs list
- ROM info panel showing parsed header (M00-04)
- macOS `.app` bundle; Windows exe with icon

## Acceptance criteria

- [ ] App launches on both platforms and renders at the display refresh rate
- [ ] Panels can be docked/undocked; layout restored on restart
- [ ] Opening a ROM populates the ROM info panel
- [ ] Window is crisp on Retina / 150% scaling displays

## Test plan

- Manual smoke test on both OSes
- Screenshot attached to the PR
