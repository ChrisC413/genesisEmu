# M04-07 · Z80 system integration

**Milestone:** [M04 – Input, Z80, playable](README.md)  
**Status:** Done  
**Depends on:** M04-05  
**Spec refs:** [03-cpu-z80](../../docs/specs/03-cpu-z80.md#requirements), [04-memory-bus](../../docs/specs/04-memory-bus.md#z80-memory-map)

## Story

As a **developer**, I want the Z80 running in the scheduler with bus arbitration, bank window, interrupts and sound-chip ports, so that games' Z80 drivers run and communicate with the 68k.

## Scope

- Catch-up execution per scanline
- BUSREQ/RESET halting
- Bank register serial writes; 68k window with stall penalties and translated-address reporting (CZ-6)
- V-blank /INT for one line
- YM2612/PSG port writes forwarded to stubs (register log)

## Acceptance criteria

- [x] Sonic 2/S3K Z80 drivers run without hanging the 68k (handshakes complete)
- [x] Register-write log shows plausible YM writes during title music

## Test plan

- `[rom]` boot tests extended
- Unit tests for bank register

## Notes

- `Machine` runs the Z80 in catch-up fashion to the end of every scanline and before every BUSREQ / RESET
  write (so handshakes see the Z80 at the current time). The Z80 runs only while neither BUSREQ nor RESET
  is asserted; the 68k owns the Z80 bus (and `$A11100` bit 8 reads 0) only while BUSREQ is asserted and
  RESET released (replaces M02's grant that ignored reset). Asserting RESET resets the Z80 CPU.
- Bank register: 9 serial writes to `$6000`; the `$8000` window reads/writes the 68k bus at
  `bank << 15`. Each access charges 3 Z80 wait states and 11 68k stall cycles (`MachineConfig::z80Window`)
  and is reported through the new `Hooks::onZ80Window` with the translated 68k address (CZ-6).
- V-blank /INT: the VDP holds `z80Int` for the first V-blank line; the Z80 takes IM 1 once per frame.
- YM2612 / PSG writes from either CPU go to register stubs and `audio::RegisterLog` (frame, line, CPU, PC,
  chip, port, register, value); `gen_cli run --reg-log <csv>` exports it.
- Verified: all three games run 3,000+ frames of attract mode and scripted play with no faults; Sonic 1
  logs 27k YM writes in 500 frames (mostly Z80 DAC writes to `$2A` = the "SEGA" sample, plus 68k key-ons),
  Sonic 2 25k (DAC + key-on `$28`), S3K 100k+ (DAC, 266 key-ons in the window, PSG). Unit tests in
  tests/unit/z80_system_test.cpp cover BUSREQ/RESET, execution, the bank window and hook, the /INT and the
  log.
