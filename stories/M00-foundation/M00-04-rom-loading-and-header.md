# M00-04 · ROM loading and header parsing

**Milestone:** [M00 – Foundation](README.md)  
**Status:** Done  
**Depends on:** M00-01  
**Spec refs:** [04-memory-bus](../../docs/specs/04-memory-bus.md#cartridge-loading)

## Story

As a **reverse engineer**, I want to open any Genesis ROM dump and see its identity and header, so that I always know exactly which ROM revision I'm analyzing.

## Scope

- Load `.bin/.md/.gen/.smd`; detect and de-interleave SMD
- Parse header `$100–$1FF` (titles, serial, checksum, region, ROM/RAM ranges, SRAM declaration)
- Compute header checksum, SHA-1 and CRC32
- Known-ROM table: SHA-1 → game + revision for the target games

## Acceptance criteria

- [x] Interleaved SMD and plain BIN of the same game yield the same SHA-1 — synthetic-ROM unit test and `cli.info_smd_matches_bin`
- [x] Header checksum mismatch is reported but doesn't block loading — unit test `[cart]`
- [x] Target-game ROMs are identified by name and revision — verified 2026-10-24 with `tools/validate-roms.ps1` against the user's ROM folder: Sonic 1 REV01, Sonic 2 REV01, Sonic 3 USA and Europe, and standalone Sonic & Knuckles are identified; S&K + Sonic 3 (USA / Europe) hash to the known combined S3K entries. Modified dumps (a patched Sonic 2 REV01, a 3-byte-modified S&K, an unknown Sonic 1, Universal Soldier) correctly stay unidentified. Master System `.sms` files are rejected as "Not a Mega Drive/Genesis ROM"
- [x] Files > 4 MB or < 512 bytes are rejected with a clear error — unit test `[cart]` and `cli.info_too_small`

## Test plan

- Unit tests with synthetic ROMs (generated in-test) for header parsing and SMD de-interleave
- `[rom]` test identifies the user's Sonic ROMs and checks the S&K + Sonic 3 lock-on hashes
- SMD vs BIN is decided by content (signature, or which layout has "SEGA" at `$100`); the `.smd` extension only breaks ties, because several of the user's `.smd` files are plain images

## Notes

Q5 is resolved by D-023: the canonical set is Sonic 1 REV01, Sonic 2 REV01 and S3K = S&K + Sonic 3 USA.
