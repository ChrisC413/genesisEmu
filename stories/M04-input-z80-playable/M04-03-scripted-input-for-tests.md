# M04-03 · Scripted input for tests

**Milestone:** [M04 – Input, Z80, playable](README.md)  
**Status:** Draft  
**Depends on:** M04-01  
**Spec refs:** [13-testing-strategy](../../docs/specs/13-testing-strategy.md#scripted-input-format-tests-only)

## Story

As a **developer**, I want the minimal scripted-input file format in `gen_cli`, so that regression tests can play games deterministically.

## Scope

- Parser for the format in spec 13
- Checkpoints for frame and state hashes
- Not a user-facing movie feature (D-007)

## Acceptance criteria

- [ ] Script that starts Sonic 1 and walks right reaches the same frame hash every run on both OSes

## Test plan

- `[rom]` CTest
