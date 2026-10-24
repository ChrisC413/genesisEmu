# M03-06 · Window plane

**Milestone:** [M03 – VDP video](README.md)  
**Status:** Draft  
**Depends on:** M03-04  
**Spec refs:** [05-vdp](../../docs/specs/05-vdp.md#rendering)

## Story

As a **developer**, I want the window plane replacing plane A in the configured region, so that HUD-style fixed layers render correctly.

## Scope

- Regs 17/18 left/right/up/down regions
- Window ignores scroll
- Window bug (H-scroll interaction) approximated

## Acceptance criteria

- [ ] Test ROM window regions render correctly
- [ ] Sonic 2 2P / menus using window look right

## Test plan

- Test ROM golden frame
