# M04-01 · Controller port protocol (3- and 6-button)

**Milestone:** [M04 – Input, Z80, playable](README.md)  
**Status:** Draft  
**Depends on:** —  
**Spec refs:** [04-memory-bus](../../docs/specs/04-memory-bus.md#io-registers), [07-frontend-platform](../../docs/specs/07-frontend-platform.md#input)

## Story

As a **developer**, I want the TH-line controller protocol for 3- and 6-button pads, so that games read input correctly.

## Scope

- TH select multiplexing; 6-button extra cycles with timeout reset
- Per-port pad type from config
- Input latched once per frame from the host (determinism)

## Acceptance criteria

- [ ] Unit tests replay the read sequence games use
- [ ] Sonic games detect a 3-button pad

## Test plan

- Unit tests
