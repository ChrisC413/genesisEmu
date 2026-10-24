# M12-03 · LED panel and device event log

**Milestone:** [M12 – Sonic 2 cartridge hardware mod: emerald LEDs](README.md)  
**Status:** Draft  
**Depends on:** M12-02  
**Spec refs:** [16-cartridge-hardware-mod](../../docs/specs/16-cartridge-hardware-mod.md#emulator-support)

## Story

As a **reverse engineer**, I want a panel showing the seven emerald LEDs with a write log (frame, PC, value), so that I can see exactly when and from where the game writes.

## Scope

- LEDs in emerald colors
- Event log; clear button; read-back toggle
- Click log entry → disassembly at PC

## Acceptance criteria

- [ ] Writing `$01` lights LED 1 and logs the PC

## Test plan

- Manual UI test
