# M03-08 · Layer priority and shadow/highlight

**Milestone:** [M03 – VDP video](README.md)  
**Status:** In progress (loop priority not verified frame by frame)  
**Depends on:** M03-05, M03-07  
**Spec refs:** [05-vdp](../../docs/specs/05-vdp.md#rendering)

## Story

As a **developer**, I want per-pixel priority composition and shadow/highlight mode, so that sprites go behind/in front of terrain correctly and S/H effects work.

## Scope

- Six-level priority composition
- Shadow/highlight with palette 3 color 14/15 operators

## Acceptance criteria

- [ ] Sonic loop-de-loops (priority swap) correct — **partly checked**: in the Sonic 1 GHZ demo (frame 2800) Sonic is drawn between the loop's back and the grass in front, as expected, but passing behind the loop's front was not followed frame by frame
- [x] S/H test ROM matches golden — verified with synthetic unit tests (plane-priority shading, highlight / shadow operators, high-priority sprites, color 14) instead of a test ROM

## Test plan

- Golden frames

## Notes

- Composition order: sprite high, A high, B high, sprite low, A low, B low, backdrop.
- Shadow / highlight: a pixel is shadowed when neither plane has its priority bit set (even on transparent
  pixels); palette 3 color 14 / 15 sprite pixels are highlight / shadow operators on what lies beneath
  (only where the sprite would be visible); high-priority sprite pixels and color 14 of palettes 0-2 are
  never shaded.
- Layer toggles and high/low priority filters: `Vdp::layers()`.
