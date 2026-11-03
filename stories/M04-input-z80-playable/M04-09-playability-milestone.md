# M04-09 · Playability milestone

**Milestone:** [M04 – Input, Z80, playable](README.md)  
**Status:** Implemented; human playtest pending (Chris)  
**Depends on:** M04-02, M04-03, M04-07, M04-08  
**Spec refs:** [00-overview](../../docs/specs/00-overview.md#success-criteria-by-phase)

## Story

As a **player**, I want to play the first act of each target game at full speed, so that the emulator core is proven before building the workbench on top.

## Scope

- Performance profiling and fixes
- Bug triage list of visual issues found while playing

## Acceptance criteria

- [ ] GHZ1, EHZ1, AIZ1 completed by a human without visible glitches
- [x] ≤ 25% of one core used at 60 fps on the reference machine (no instrumentation)
- [x] Scripted-input regression for each act start

## Test plan

- Manual playtest checklist in the PR
- Benchmark results recorded

## Notes

- **Human playtest not done** (the agent cannot play): "GHZ1, EHZ1, AIZ1 completed by a human without visible
  glitches" stays unchecked for Chris.
- Scripted stand-ins (tests/scripts/, `[rom]` test `Scripted input reaches the recorded checkpoints`):
  - Sonic 1: START on the title at frame 620, then run right with jumps. Frame 1150 shows Sonic running in
    GHZ1 at 0:05 next to a Motobug; frame 1300 shows him knocked back on the bridge (hurt pose, rings lost),
    i.e. collision and damage work.
  - Sonic 2: START at 950 (earlier presses during the title animation are ignored by the game). Frame 1500:
    1P EHZ1 at 0:06, Sonic mid-jump, Tails following.
  - S3K: START on the title, NO SAVE on data select, then right + jumps after the AIZ intro. Frame 2400:
    Sonic in AIZ1 at 0:06 with score 100 and 7 rings.
- Performance (Windows 11, i7-11700KF, Release): the core takes 0.55-0.75 ms per frame (gen_cli: 0.69 / 0.63
  / 0.72 ms per frame for the three scripted runs). The whole app (emulation + ImGui + OpenGL upload, incl.
  start-up) used 2.4 ms of CPU per frame, i.e. ~14% of one core at 60 fps. No optimisation was needed.
- Bug triage (from the sampled frames): no visible glitches found. Worth a look by eye: the S3K data-select
  fade-in (frame ~820 of s3k_aiz1) shows a half-built screen for a frame or two, which is probably what the
  hardware shows mid-load.
