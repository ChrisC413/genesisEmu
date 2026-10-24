# M11-09 · Donor sound porting

**Milestone:** [M11 – Special-stage splicing](README.md)  
**Status:** Draft  
**Depends on:** M11-03  
**Spec refs:** [15-subsystem-splicing](../../docs/specs/15-subsystem-splicing.md#4-splice-plan-target-sonic-2)

## Story

As a **developer**, I want donor music/SFX converted into the target's sound driver format or mapped to target IDs, so that the spliced stage sounds right.

## Scope

- Sound-ID translation table
- Sonic 1 68k SMPS → Sonic 2 Z80 SMPS conversion
- Fallback mapping to existing S2 tracks (Q17)

## Acceptance criteria

- [ ] S1 special-stage music plays in S2 (converted or mapped)
- [ ] SFX IDs translated correctly

## Test plan

- `[rom]` audio-hash test of the converted track
