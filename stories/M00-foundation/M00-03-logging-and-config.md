# M00-03 · Logging, configuration and platform paths

**Milestone:** [M00 – Foundation](README.md)  
**Status:** Draft  
**Depends on:** M00-01  
**Spec refs:** [07-frontend-platform](../../docs/specs/07-frontend-platform.md#configuration-and-paths), [07-frontend-platform](../../docs/specs/07-frontend-platform.md#logging)

## Story

As a **developer**, I want structured logging and a JSON config stored in the right per-platform location, so that all later subsystems have a consistent place for settings and diagnostics.

## Scope

- spdlog sinks: console + rotating file; per-subsystem log levels
- Config load/save (JSON) with defaults and schema versioning
- Platform paths: `%APPDATA%\genesisEmu` and `~/Library/Application Support/genesisEmu`
- Per-ROM data folder keyed by SHA-1

## Acceptance criteria

- [ ] Missing config → defaults written on first run
- [ ] Unknown keys preserved on save
- [ ] Log level per subsystem configurable from the config file
- [ ] Paths resolve correctly on both platforms

## Test plan

- Unit tests for config round-trip and migration
- Manual check of paths on both OSes
