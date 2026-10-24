# M12-08 · Physical cartridge validation

**Milestone:** [M12 – Sonic 2 cartridge hardware mod: emerald LEDs](README.md)  
**Status:** Draft  
**Depends on:** M12-05, M12-07  
**Spec refs:** [16-cartridge-hardware-mod](../../docs/specs/16-cartridge-hardware-mod.md)

## Story

As a **reverse engineer**, I want the modded ROM and test ROM validated on the physical LED cartridge, so that the hardware mod is done.

## Scope

- Test ROM on the cart
- Modded S2 (with and without splices) on the cart
- Power-cycle persistence check

## Acceptance criteria

- [ ] Each emerald lights its LED on hardware; state persists across power cycles; stock behavior otherwise unchanged

## Test plan

- Manual hardware test, report in PR

## Notes

Depends on the hardware build, which is outside this repository.
