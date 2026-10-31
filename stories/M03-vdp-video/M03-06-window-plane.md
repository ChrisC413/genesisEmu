# M03-06 · Window plane

**Milestone:** [M03 – VDP video](README.md)  
**Status:** In progress (game-level check needs M04 input)  
**Depends on:** M03-04  
**Spec refs:** [05-vdp](../../docs/specs/05-vdp.md#rendering)

## Story

As a **developer**, I want the window plane replacing plane A in the configured region, so that HUD-style fixed layers render correctly.

## Scope

- Regs 17/18 left/right/up/down regions
- Window ignores scroll
- Window bug (H-scroll interaction) approximated

## Acceptance criteria

- [x] Test ROM window regions render correctly — verified with a synthetic VRAM unit test instead of a test ROM (left / right of the H position, above / below the V position, scrolling ignored, debug toggle)
- [ ] Sonic 2 2P / menus using window look right — **not verified**: needs menu navigation (M04 input)

## Test plan

- Test ROM golden frame

## Notes

- The window covers a whole line when the vertical condition (reg 18) holds, otherwise the horizontal
  range from reg 17. Window name table: 64 cells wide in H40, 32 in H32; no scrolling.
- The window / plane A boundary bug (plane A fine scroll garbling the first column after the window) is
  not modeled.
