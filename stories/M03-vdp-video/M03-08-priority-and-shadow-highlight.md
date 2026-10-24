# M03-08 · Layer priority and shadow/highlight

**Milestone:** [M03 – VDP video](README.md)  
**Status:** Draft  
**Depends on:** M03-05, M03-07  
**Spec refs:** [05-vdp](../../docs/specs/05-vdp.md#rendering)

## Story

As a **developer**, I want per-pixel priority composition and shadow/highlight mode, so that sprites go behind/in front of terrain correctly and S/H effects work.

## Scope

- Six-level priority composition
- Shadow/highlight with palette 3 color 14/15 operators

## Acceptance criteria

- [ ] Sonic loop-de-loops (priority swap) correct
- [ ] S/H test ROM matches golden

## Test plan

- Golden frames
