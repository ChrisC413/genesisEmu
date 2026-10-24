# M07-01 · Coverage recorder and shadow arrays

**Milestone:** [M07 – ROM & memory exploration](README.md)  
**Status:** Draft  
**Depends on:** —  
**Spec refs:** [09-coverage-and-rom-map](../../docs/specs/09-coverage-and-rom-map.md#recorded-data-per-rom-byte), [01-architecture](../../docs/specs/01-architecture.md#instrumentation-hook-api)

## Story

As a **developer**, I want a coverage recorder attached to the hook API with per-byte shadow arrays for ROM, 68k RAM and Z80 RAM, so that every exploration feature has accurate raw data.

## Scope

- Flags + counters per spec 09
- Fetch vs data read; opcode vs operand bytes (needs instruction length)
- Z80 bank-window reads attributed to ROM
- Toggle on/off at runtime

## Acceptance criteria

- [ ] Unit tests: synthetic ROM with known code/data produces exact flags
- [ ] Performance budget met with coverage on

## Test plan

- Unit + perf tests
