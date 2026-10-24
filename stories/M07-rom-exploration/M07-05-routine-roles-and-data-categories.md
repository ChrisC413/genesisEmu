# M07-05 · Routine roles and data categories (graphics, sprite, palette, level, sound, not used)

**Milestone:** [M07 – ROM & memory exploration](README.md)  
**Status:** Draft  
**Depends on:** M07-01  
**Spec refs:** [09-coverage-and-rom-map](../../docs/specs/09-coverage-and-rom-map.md#routine-roles-reader-attribution), [09-coverage-and-rom-map](../../docs/specs/09-coverage-and-rom-map.md#data-categories)

## Story

As a **reverse engineer**, I want data bytes classified by what they are — graphics, sprite, palette, level, sound, compressed, other, not used, so that I can see where every kind of asset lives in the ROM.

## Scope

- Consume `onDma` events → category by destination
- Routine roles (decompressor+format, sprite builder, mapping/DPLC loader, palette loader, level loader, sound driver, HUD/text) tagged from UI, analysis or accepted LLM proposals
- Reads by role PCs set READ_BY_ROLE + category
- Z80 bank-window reads and Z80-RAM copies → sound
- Category priority per spec 09; `not-used` = untouched in all included layers and not static code

## Acceptance criteria

- [ ] After tagging Sonic 1's Nemesis decompressor and BuildSprites, GHZ art shows as graphics (compressed) and Sonic's mappings as sprite
- [ ] DAC samples and the Z80 driver show as sound
- [ ] Category byte totals reported in the legend

## Test plan

- Unit tests with a synthetic ROM
- `[rom]` spot checks against reference labels

## Notes

Sonic games often copy palettes to a RAM buffer and DMA from RAM; add 'RAM-buffer back-tracing' as a follow-up if needed.
