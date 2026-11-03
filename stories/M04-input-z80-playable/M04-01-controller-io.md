# M04-01 · Controller port protocol (3- and 6-button)

**Milestone:** [M04 – Input, Z80, playable](README.md)  
**Status:** Done  
**Depends on:** —  
**Spec refs:** [04-memory-bus](../../docs/specs/04-memory-bus.md#io-registers), [07-frontend-platform](../../docs/specs/07-frontend-platform.md#input)

## Story

As a **developer**, I want the TH-line controller protocol for 3- and 6-button pads, so that games read input correctly.

## Scope

- TH select multiplexing; 6-button extra cycles with timeout reset
- Per-port pad type from config
- Input latched once per frame from the host (determinism)

## Acceptance criteria

- [x] Unit tests replay the read sequence games use
- [x] Sonic games detect a 3-button pad

## Test plan

- Unit tests

## Notes

- `bus::Io` implements the TH multiplexing for 3- and 6-button pads (6-button identification phase, X/Y/Z/Mode
  phase, ~1.5 ms TH timeout measured in master clocks), a pad type per port (`Machine::setPadType`, `--pad`
  in gen_cli) and `Machine::setInput()`, latched into the I/O chip at each frame boundary.
- tests/unit/io_test.cpp replays the Sonic 1/2 two-phase read and the full 6-button sequence.
- All three games react to the scripted 3-button input (tests/scripts/*.txt): Sonic 1 and 2 start a game with
  START and Sonic runs and jumps; S3K navigates its data-select screen.
