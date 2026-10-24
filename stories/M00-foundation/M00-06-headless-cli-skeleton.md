# M00-06 · Headless CLI runner skeleton

**Milestone:** [M00 – Foundation](README.md)  
**Status:** In progress  
**Depends on:** M00-04  
**Spec refs:** [01-architecture](../../docs/specs/01-architecture.md#modules)

## Story

As a **developer**, I want a `gen_cli` executable with subcommands for info, run and test, so that automation and CI can drive the core without a UI.

## Scope

- Argument parsing; subcommands `info`, `run --frames N`, later `trace`, `coverage`, `export`
- Exit codes: 0 ok, 1 usage, 2 load error, 3 check failure

## Acceptance criteria

- [ ] `gen_cli info --rom x.bin` prints header, SHA-1, identified game
- [ ] `gen_cli run` exists (no-op until M02) and returns 0
- [ ] `--help` documents every subcommand

## Test plan

- CTest invokes `gen_cli info` on a synthetic ROM
