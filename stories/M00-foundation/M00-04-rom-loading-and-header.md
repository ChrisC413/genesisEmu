# M00-04 · ROM loading and header parsing

**Milestone:** [M00 – Foundation](README.md)  
**Status:** Draft  
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

- [ ] Interleaved SMD and plain BIN of the same game yield the same SHA-1
- [ ] Header checksum mismatch is reported but doesn't block loading
- [ ] Target-game ROMs are identified by name and revision
- [ ] Files > 4 MB or < 512 bytes are rejected with a clear error

## Test plan

- Unit tests with synthetic ROMs (generated in-test) for header parsing and SMD de-interleave
- `[rom]` test identifies the user's Sonic ROMs

## Notes

Q5 decides which revisions are canonical.
