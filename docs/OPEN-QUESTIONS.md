# Open Questions

Each question names the milestone it blocks and a **recommended default** that the specs currently assume.
When a question is answered, it moves to the resolved table and into [DECISIONS.md](DECISIONS.md).

## Open

| # | Question | Blocks | Recommended default |
|---|----------|--------|---------------------|
| Q9 | **Rewind / input movies later?** | — | *TBD.* Out of scope per D-007; the deterministic core (D-011) keeps both cheap to add. |
| Q12 | **Which special stages go into which game?** Sonic 2 is the target (the hardware mod is for Sonic 2). Donors could be the Sonic 1 rotating maze and the Sonic 3 / S&K Blue Sphere stages. Should we also support other targets? | M11 | Target: Sonic 2 only. First donor: Sonic 1 (smaller, 68k sound driver, well documented); second donor: S3K Blue Sphere. |
| Q13 | **How is a spliced stage chosen in-game?** Replace the S2 halfpipe, rotate between stages, or pick per emerald / per entry? | M11 | A configurable stage table indexed by emerald number, defaulting to the original order. Shipped as a build-time option. |
| Q14 | **Retry-after-failure behavior?** Unlimited retries, cost (rings/lives), which stages (special stages only, or bosses/acts too), and the UI (prompt vs. button combo). | M10 | Special stages only; "Retry? Yes/No" prompt on failure; retry costs nothing; build-time option to limit retries per entry. |
| Q15 | **Hardware mod: does the game ever read the cartridge state back?** For example, restoring emeralds on power-on from non-volatile storage on the cart, or is it write-only to drive LEDs? | M12 | Write + read-back: the cart latches the emerald bitmask in non-volatile storage; the game can read it, but by default only writes. The LEDs show the latched state. |
| Q16 | **Hardware mod: what triggers an LED?** Collecting an emerald (special stage success) only, or also other milestones (all emeralds → Super Sonic, game completion)? | M12 | One LED per emerald (7 bits), set on special-stage success. Bit 7 is reserved for "all emeralds". |
| Q17 | **How should donor music/SFX be handled?** Sonic 1 uses a 68k SMPS driver while Sonic 2 uses a Z80 SMPS driver with different formats and sound IDs. | M11 | Convert donor music/SFX data to S2's SMPS format at build time; fall back to reusing an existing S2 track if conversion fails. |
| Q18 | **When should the emerald LEDs reset?** Never (permanent achievements), on a new game, or only via a hidden clear (the `CONTROL` sequence)? | M12 | Only via the hidden clear sequence (e.g. triggered from a cheat/debug menu); LEDs otherwise persist across games and power cycles. |

## Resolved (2026-10-24)

| # | Answer | Decision |
|---|--------|----------|
| Q5 | Canonical set (from the user's ROM folder, 2026-10-24): **Sonic 1 REV01** (`69e10285…`), **Sonic 2 REV01** (`8bca5dce…`), **S3K = Sonic & Knuckles + Sonic 3 USA combined** (`cfbf98c3…`, NTSC; standalone S&K `88d6499d…` + S3 USA `75e9c470…`). Secondary (PAL): S3 Europe (`2ff45bb0…`), giving S3K Europe (`45ca7973…`). The folder also holds modified or unknown dumps that stay unidentified (a patched Sonic 2 REV01 SMD `9e1834a8…`, an S&K with 3 changed bytes `e86b85ee…`, an unknown Sonic 1 with a REV00 header `f83bdaab…`, and Universal Soldier) and three Master System `.sms` files, which the loader rejects. | D-023 |
| Q1 | Use ymfm | D-013 |
| Q2 | MIT | D-014 |
| Q3 | Windows 11, macOS 13+ | D-015 |
| Q4 | GitHub + GitHub Actions; validate macOS builds locally; no paid GHA features | D-016 |
| Q6 | No widescreen. Modify special stages, splice in special stages from other Sonic games, add retry after failure, and a future Sonic 2 cartridge hardware mod: the game writes to the cart on each emerald collected, and LEDs light | D-017, D-022 |
| Q7 | Yes | D-018 |
| Q8 | The goal is to enable splicing in special stages | D-019 |
| Q10 | Yes, NTSC first | D-020 |
| Q11 | Tiers: once, >1, per frame, > per frame; plus data categories (sprite, sound, not used, etc.) | D-021 |
