# M06-00b · Bug: Sonic 2 Chemical Plant liquid tints the whole screen

**Milestone:** [M06 – Debugger & freeze execution](README.md)  
**Status:** Done  
**Depends on:** M06-00a (snapshots used to reproduce it)  
**Spec refs:** [05-vdp](../../docs/specs/05-vdp.md#interrupts-and-timing), [13-testing-strategy](../../docs/specs/13-testing-strategy.md)

## Story

As a **player**, I want the Mega Mack in Sonic 2's second zone (Chemical Plant) to look like it does on a
console, so that the level is playable and the emulator can be trusted for the reverse-engineering work.

## Symptom

In Chemical Plant Zone act 2 the whole picture, HUD included, was drawn with the underwater (pink / purple)
palette while the Mega Mack was below the screen, and the act's title card was purple instead of red, blue and
yellow. Frames with the liquid surface on screen were split correctly. Emerald Hill act 2 has no water H-int
and was not affected.

## Root cause

Sonic 2 keeps the H-int counter (VDP register 10) at the water line, and when the water is below the screen
it writes 223 (`$8ADF`, from `Hint_counter_reserve` in its V-int handler). The counter then underflows after
the last visible line, 223. The water H-int handler (`$000F54`) checks `Hint_flag` (`$FFF644`, set by every
V-int) and, when set, clears it and copies `Underwater_palette` (`$FFF080`) into CRAM.

On a console that H-int fires at the end of line 223. The V-int comes later, at H counter $00 → $01 of line
224. That gap is about 107 68k cycles in H40, and the handler masks interrupts (`move #$2700,sr` at
`$001000`) inside it. So the order is H-int (underwater palette into CRAM) and then V-int (the normal palette
is DMA'd back), and the next frame starts with the normal palette.

The VDP model raised both interrupts at the same instant: the H-int at `endLine(223)` and the V-int at
`beginLine(224)`. The 68k took the higher-priority V-int first. The H-int was still pending and ran after the
V-int's `rte`, at line 238, with `Hint_flag` just set again. `PalToCRAM` then left the underwater palette in
CRAM for the whole next frame.

Evidence (`gen_cli trace --load-state` from a CPZ2 snapshot at frame 2900, before the fix):

```
2900 224 ... M68K 003388   interrupt level 6
2900 226 ... M68K 000748   move.w ($FFFFF624).w,(a5)   | W.W C00004=8ADF        reg 10 = 223
2900 238 ... M68K 003388   interrupt level 4           H-int serviced after the V-int
2900 238 ... M68K 001018   move.l #$C0000000,$4(a1)    CRAM write from $FFF080 (underwater palette)
```

A second, intermittent case remained after delaying the V-int. The Z80's bank-window accesses (DAC samples)
stall the 68k, and the scheduler charges those stalls in one lump when it catches the Z80 up at the line end.
In frame 3798 that lump (2 × 11 cycles) pushed the 68k's H-int entry 154 MCLK past the boundary, and the V-int
again arrived one instruction before `move #$2700,sr`.

## Fix

- The VDP arms V-int at the start of line 224 and the scheduler raises it `vintDelayMclk()` later: 46 H40
  counter units (≈749 MCLK) or 39 H32 units (780 MCLK). That is the distance from the V counter increment
  (H $A5 / $85), where the scheduler's line boundary and H-int sit, to H $00 → $01, where the F flag is set.
  Positions are from Nemesis' VDP timing measurements (SpritesMind forum, "VDP HV counter/Status register
  progression and timings", 2010). The Z80 /INT stays at the start of the line.
- The delay is counted from the boundary plus any Z80 stall charged to the 68k at that boundary, so the
  H-int → V-int distance the 68k sees is the hardware one.
- `vintArmed` (VDP state) and the V-int time (scheduler) are part of the state hash and of save states.

## Acceptance criteria

- [x] CPZ act 2: no tinted frame with the Mega Mack below the screen (a 460-frame scan, frames 2700-5000 every
  5th, finds only the purple-free title card frames, which are drawn in their correct colours)
- [x] CPZ act 2 with the Mega Mack on screen: normal palette above the surface, underwater palette below it
- [x] Unit test: with register 10 = 223 the H-int is taken before the V-int, both early in line 224
- [x] Regression script `tests/scripts/sonic2_cpz2.txt` (level select → CPZ2 → down to the Mega Mack) with
  frame-hash checkpoints for the title card, a frame with the water below the screen and one with the
  surface on screen
- [x] Other games' goldens checked; changes explained in the `[golden-update]` commit

## Test plan

- `tests/unit/machine_test.cpp` "An H-int at the end of the last active line is taken before the V-int"
  (boot ROM, CI).
- `tests/unit/vdp_test.cpp` V-int arming and `raiseVint`.
- `[rom]` `tests/roms/script_test.cpp` runs `sonic2_cpz2.txt` with the other scripted regressions.

## Notes

- Golden changes: every state hash and audio hash moves because the V-int is now ~107 68k cycles later in
  every frame. All existing scripted frame-hash checkpoints are unchanged. One M03 golden frame changed: Sonic
  2's 2P attract demo at frame 1400, by one row (field line 108). Its 2P split handler's display-off write now
  lands 80 MCLK after a line boundary instead of just before it. That is the line-granular register model of
  spec 05, not a regression.
- The HV counter approximation still treats the line boundary as H = 0 (spec 05); aligning it with the
  V-counter increment (H $A5 / $85) would be more accurate but changes timing in every game and is left for
  later.
- Reaching CPZ uses Sonic 2's level select (options sound test 19, 65, 09, 17, then A+START on the title
  screen). Generated scripts and save states stay outside the repository; only the input script is committed.
