# 16 · Sonic 2 Cartridge Hardware Mod: Emerald LEDs

A modified Sonic 2 cartridge has seven LEDs, one per Chaos Emerald. A modded Sonic 2 ROM writes to a
cartridge register each time an emerald is collected; logic on the cartridge latches the value, lights the
LED, and keeps it lit in non-volatile storage.

This spec is the **interface contract between the ROM mod, the emulator, and the hardware**. The
hardware build is outside this repository; this spec is what it's built against.

## Register interface (proposal, D-022)

The registers sit in the `/TIME` range (`$A13000–$A130FF`), which the console strobes for cartridge-side
registers. `$A130F0–$A130FF` is avoided (SRAM control / SSF2 mapper / flash-cart conventions). Byte access
on **odd** addresses, lower data bus `D0–D7`, strobed by `/LWR`.

| Address | Name | Access | Description |
|---------|------|--------|-------------|
| `$A130E1` | `EMERALD_SET` | W | Bits 0–6: OR into the emerald latch (set-only; writing 0 bits never clears). Bit 7: reserved ("all emeralds", Q16). |
| `$A130E3` | `EMERALD_STATE` | R | Current latch value (if read-back is implemented, Q15). Reads return open bus on a cartridge without the mod. |
| `$A130E5` | `DEVICE_ID` | R | `$E7` when the device is present (optional presence detection). |
| `$A130E7` | `CONTROL` | W | Clear sequence: write `$A5` then `$5A` on consecutive writes clears the latch. Any other value resets the sequence. |

Rules:

- The ROM **must not depend** on the device: on a stock cartridge, an emulator without the device, or a flash cart, the
  game behaves identically (writes go nowhere, reads are ignored unless `DEVICE_ID` matches).
- Set-only semantics make repeated or duplicate writes harmless.
- The clear sequence exists for testing and for the "reset LEDs" policy (open question Q18).
- **Flash-cart compatibility**: the chosen flash cart must be checked for its own `$A130xx` registers; the address block
  may move if it conflicts (update this table and D-022 together).

## Game-side behavior (ROM mod)

- When the special-stage result awards emerald *n* (0–6), the ROM writes `1 << n` to `EMERALD_SET`. This happens in the
  results path for **native and spliced** stages (spec 15 exit glue calls the same routine).
- Optional (Q15): on boot, if `DEVICE_ID == $E7`, read `EMERALD_STATE` to restore emeralds. Default: write-only.
- The write is a single `move.b #mask,($A130E1).l` inside a small routine `CartLed_SetEmerald` so it is easy to find,
  patch and breakpoint.

## Hardware notes (informative)

- Signals needed: `/TIME`, `/LWR`, `A1–A7`, `D0–D7`, `/VRES` (reset), +5 V, GND. All 5 V logic.
- Write detection must catch a single 68k bus cycle (~ 4 clocks at 7.67 MHz ≈ 500 ns, with `/LWR` asserted for part of it):
  a CPLD (or 5 V-tolerant logic) is the safe choice for the latch; a microcontroller can then drive LEDs and storage.
- Read-back (`EMERALD_STATE`, `DEVICE_ID`) requires driving `D0–D7` within the console's access time; cartridge-side
  wait states are not available, which again favors a CPLD.
- Non-volatile storage (FRAM / EEPROM) holds the latch across power cycles.
- If splicing grows the ROM past 1 MB (spec 15), the cartridge needs a ROM/flash chip of 2 MB or 4 MB with matching
  address decoding.

## Emulator support

| Feature | Description |
|---------|-------------|
| `EmeraldLedDevice` | An `ICartDevice` (spec 04) implementing the register table, persisted to `<rom-sha1>.cartdev.json`, included in save states. Enabled per ROM SHA-1 or manually. |
| LED panel | Seven LEDs in emerald colors, with a per-write event log (frame, PC, value), a "clear" button, and a toggle for "read-back implemented". |
| Prototype mode | Before the ROM mod exists: drive the virtual LEDs from a **RAM watch** on Sonic 2's emerald state in an unmodified ROM, to validate events and LED UX early. |
| Debugging | Device accesses go through the normal hooks: watchpoints on `$A130E1`, traces, coverage. Strict mode (spec 14) warns on `$A130xx` writes no device claims. |
| Testing | Scripted-input checkpoint `cartdev emerald_leds == <mask>` (spec 13); a scripted run collects one emerald and asserts the mask. |
| Hardware test ROM | A tiny homebrew ROM (built in this repo) that walks the LEDs, exercises set/clear, and shows read-back/ID results on screen — for bringing up the physical cartridge. |
