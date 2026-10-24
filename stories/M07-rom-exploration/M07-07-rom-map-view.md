# M07-07 · Color-coded ROM map view

**Milestone:** [M07 – ROM & memory exploration](README.md)  
**Status:** Draft  
**Depends on:** M07-02, M07-03, M07-05, M07-06  
**Spec refs:** [09-coverage-and-rom-map](../../docs/specs/09-coverage-and-rom-map.md#rom-map-view)

## Story

As a **reverse engineer**, I want a zoomable, color-coded map of the whole ROM with legend and overlays, so that I can see at a glance what every part of the ROM is.

## Scope

- Linear (and optional Hilbert) layout, zoom 1 B/px–1 KB/px
- Incremental texture updates ≤ 4 Hz
- Colors per spec 09 for the four execution tiers and all data categories; colorblind palette
- Overlays: category, execution tier, heat, watch layer, layer diff, subsystem footprint (M11), touched-this-frame
- Legend with byte counts/percentages; per-CPU, per-tier and per-category filters

## Acceptance criteria

- [ ] Map updates live while playing without dropping below 60 fps
- [ ] Legend percentages sum to 100%
- [ ] Switching overlays is instant

## Test plan

- Manual UI test; screenshot in PR
