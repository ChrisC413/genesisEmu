# BL-01 · C static recompilation spike

**Milestone:** [BL – Backlog](README.md)  
**Status:** Draft  
**Depends on:** —  
**Spec refs:** —

## Story

As a **developer**, I want a time-boxed spike recompiling one leaf routine and one object routine of Sonic 1 to C running in lockstep with the interpreter, so that we can decide whether C recompilation is worth pursuing.

## Scope

- Translate to C over an emulated register file/memory API
- Lockstep oracle comparing RAM/VDP per frame
- Report: divergences, performance, effort estimate

## Acceptance criteria

- [ ] Both routines run in lockstep for 10 minutes of scripted play with no divergence
- [ ] Written feasibility report

## Test plan

- Lockstep harness

## Notes

Moved to the backlog by D-017 (no widescreen; mods fit the hardware model). Time-box: 2 weeks if promoted.
