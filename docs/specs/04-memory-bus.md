# 04 · Memory Bus, I/O and Cartridge

## 68000 memory map

| Range | Contents | Notes |
|-------|----------|-------|
| `$000000–$3FFFFF` | Cartridge ROM (up to 4 MB) | S3K uses the full 4 MB. SRAM may overlay `$200000–$20FFFF` when enabled. |
| `$400000–$7FFFFF` | Expansion (Sega CD) | Unmapped → open bus |
| `$800000–$9FFFFF` | 32X | Unmapped → open bus |
| `$A00000–$A0FFFF` | Z80 address space | Accessible only while the 68k holds the Z80 bus. Word access returns the byte in both halves. |
| `$A10000–$A1001F` | I/O: version, controller data/control/serial | Byte registers at odd addresses |
| `$A11100` | Z80 BUSREQ | Write bit 8 (`$0100`) = request; read bit 8 = 0 when granted |
| `$A11200` | Z80 RESET | Write bit 8 = 0 assert reset, 1 release |
| `$A130F1` | SRAM / mapper control | bit 0 = SRAM enable, bit 1 = write protect |
| `$A13000–$A130FF` (other) | Cartridge `/TIME` registers | Routed to the cartridge's **device chain**, e.g. the emerald-LED device (spec 16) |
| `$A14000` | TMSS register | Write `'SEGA'` to unlock VDP (TMSS model configurable; default: no TMSS) |
| `$C00000–$C0001F` | VDP data (`$00`), control (`$04`), HV counter (`$08`), PSG (`$11`) | Mirrored |
| `$E00000–$FFFFFF` | 64 KB work RAM, mirrored | Canonical addresses `$FF0000–$FFFFFF` |

Unmapped reads return **open bus** (last prefetched word) — approximation is acceptable. Unmapped writes are
dropped and reported to an "unmapped access" log (useful for analysis).

## Z80 memory map

| Range | Contents |
|-------|----------|
| `$0000–$1FFF` | 8 KB Z80 sound RAM (mirrored at `$2000–$3FFF`) |
| `$4000–$4003` | YM2612 (A0/D0/A1/D1), mirrored to `$5FFF` |
| `$6000` | Bank register: 9 serial writes of bit 0 build address bits 15–23 |
| `$7F11` | PSG |
| `$7F00–$7F1F` | VDP (dangerous on hardware; implement reads/writes as for 68k) |
| `$8000–$FFFF` | 32 KB window into 68k space at `bank << 15` |

## I/O registers

- **Version** (`$A10001`): bit 7 overseas (1 = export), bit 6 PAL, bit 5 no expansion unit (1), bits 0–3 version (1 = TMSS model). Region comes from config, defaulting to the cartridge header.
- **Controller ports 1/2** (`$A10003/5`, control `$A10009/B`): TH-line protocol for 3- and 6-button pads (spec 07 for mapping). Port 3 (EXT) returns idle.
- Serial registers: stubbed.

## Cartridge loading

- Accept `.bin`, `.md`, `.gen`, `.smd`. Detect and de-interleave SMD (512-byte header + 16 KB interleaved blocks).
- Parse the header at `$100–$1FF`: console name, copyright, domestic/overseas titles, serial, checksum (`$18E`), I/O support, ROM/RAM ranges, region codes.
- Compute: header checksum (sum of words from `$200`), SHA-1 and CRC32 of the de-interleaved ROM. Report checksum mismatch but still run.
- **SRAM**: honor header `RA` declaration; S3K stores SRAM on odd bytes at `$200001–$203FFF`. Persist to `<rom-sha1>.srm` in the user data directory on exit and on SRAM-disable writes.
- **Lock-on**: the combined S3K ROM is treated as a flat 4 MB image. True lock-on (two separate dumps) is a later nice-to-have.
- Mapper: implement `$A130F1`. The SSF2 bank mapper (`$A130F3–$A130FF`) is not needed for target games but should be a small isolated class if added.

### Cartridge device chain

The cartridge is modeled as ROM plus an ordered list of **cartridge devices** (`ICartDevice`) that can claim
`/TIME` addresses (`$A130xx`) and, optionally, writes to ROM space (which a ROM chip ignores but cartridge
logic can snoop). Built-in devices: SRAM controller (`$A130F1`), emerald-LED device (spec 16). Devices are
selected per ROM by SHA-1, by a header marker, or manually in the UI, and are part of save states. Every
device access goes through the normal `onAccess` hook, so breakpoints and traces see it.

## Bus requirements

| ID | Requirement |
|----|-------------|
| B-1 | All CPU accesses route through one `Bus` with a fast path for ROM and RAM (page table of 64 KB pages to handlers or direct pointers). |
| B-2 | Every access calls `onAccess` with CPU, kind (fetch/read/write), address, size, value and PC. Instruction fetches are distinguishable from data reads. |
| B-3 | Z80 BUSREQ/RESET semantics: 68k reads of Z80 space while not owning the bus return open bus and are logged as a program error. |
| B-4 | Word/long accesses to odd addresses raise a 68k address error (handled in the CPU). |
| B-5 | Byte writes to VDP data port duplicate the byte into both halves of the word (hardware behavior). |
