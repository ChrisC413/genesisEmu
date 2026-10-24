# M12 · Sonic 2 cartridge hardware mod: emerald LEDs

**Goal:** Emulate and support the custom Sonic 2 cartridge: the ROM writes to a `/TIME` register on each emerald collected; the cartridge latches it and lights one LED per emerald.

**Depends on:** M04-08 (cartridge device chain), M06 (watchpoints); M12-05+ need M10

## Testable stage (exit criteria)

In the emulator, the modded Sonic 2 ROM lights one virtual LED per emerald collected (native and spliced stages), state persists across restarts, and a hardware test ROM exercises the register. On the physical cartridge, the same ROM lights the same LEDs.

## Notes

M12-01..04 can start right after M06, in parallel with M07–M10. Q15, Q16 and Q18 shape the register semantics.

## Stories

| ID | Story | Depends on |
|----|-------|------------|
| M12-01 | [Finalize the cartridge interface control document](M12-01-interface-control-document.md) | — |
| M12-02 | [Emulated emerald-LED cartridge device](M12-02-emerald-led-device.md) | M12-01, M04-08 |
| M12-03 | [LED panel and device event log](M12-03-led-panel-and-log.md) | M12-02 |
| M12-04 | [Prototype mode: LEDs driven by RAM watch](M12-04-prototype-ram-watch-mode.md) | M12-03 |
| M12-05 | [Sonic 2 ROM mod: write to the cartridge on emerald collected](M12-05-s2-rom-emerald-writes.md) | M12-02, M10-01 |
| M12-06 | [Scripted-test support for cartridge devices](M12-06-led-scripted-tests.md) | M12-05, M04-03 |
| M12-07 | [Hardware bring-up test ROM](M12-07-hardware-test-rom.md) | M12-02 |
| M12-08 | [Physical cartridge validation](M12-08-physical-cart-validation.md) | M12-05, M12-07 |
