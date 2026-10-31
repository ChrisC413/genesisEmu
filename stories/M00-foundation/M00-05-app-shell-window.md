# M00-05 · Application shell: SDL3 window with ImGui docking

**Milestone:** [M00 – Foundation](README.md)  
**Status:** In progress  
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

- [ ] App launches on both platforms and renders at the display refresh rate — Windows: launches and renders with vsync (`app.smoke_*` CTest, local GPU); macOS *unverified*; refresh rate not measured
- [x] Panels can be docked/undocked; layout restored on restart — layout persisted in `config.json`; `app.smoke_layout_restored` checks the panels are docked after a restart. Interactive drag/undock not exercised
- [x] Opening a ROM populates the ROM info panel — smoke test loads a synthetic ROM; checked on a captured screenshot (Windows)
- [ ] Window is crisp on Retina / 150% scaling displays — *unverified*: Hi-DPI code paths exist (content-scale sizing, `FontScaleDpi`, `NSHighResolutionCapable`) but only a 100% display was available

## Test plan

- Manual smoke test on both OSes
- Screenshot attached to the PR
