# 15 · Subsystem Extraction and Special-Stage Splicing

Goal: take a special stage from a **donor** game (Sonic 1 rotating maze, S3K Blue Sphere) and make it run inside
the **target**, Sonic 2 — entered from a Sonic 2 special-stage ring and awarding a Sonic 2 emerald (Q12).

The splice is done on assembly source (D-019): the donor's exported source for the stage is turned into a
relocatable, namespaced module; generated glue code connects it to the target's export.

## Concepts

| Term | Meaning |
|------|---------|
| **Subsystem** | The set of donor resources implementing the stage: functions, data, RAM, VRAM layout, VDP settings, interrupt handlers, sound IDs |
| **Owned** vs **shared** | Owned members are used only by the subsystem; shared ones (decompressors, DMA queue, `PlaySound`, controller read, object loader) are also used by the rest of the donor game |
| **Footprint** | Measured resource usage while the subsystem runs |
| **Contract** | Entry/exit conditions and resource usage, machine-checkable (spec 11) |
| **Splice plan** | Everything needed to place the subsystem into the target: placement, symbol prefix, RAM map, dependency mapping, glue, sound mapping |

## 1. Define the subsystem (donor project)

1. **Seed from coverage**: in the donor game, arm a watch session before entering the special stage and stop it after
   the results screen. The watch layer is the seed (spec 09).
2. **Roots**: the stage's game-mode entry (e.g. Sonic 1's special-stage game mode routine) and any interrupt routines
   it installs or selects.
3. **Closure**: static call graph from the roots ∪ code executed in the seed layer by those roots' call trees, plus all
   data referenced by member code (typed data, pointer tables, compressed blocks).
4. **Classify**: each member is *owned* or *shared* — shared if it is also called or read outside the seed layer (from
   other layers/static xrefs).
5. Members, owned/shared status and confidence are stored in `subsystems` / `subsystem_members` (spec 12) and shown as a
   **subsystem footprint overlay** on the ROM map.

## 2. Measure the footprint

From the seed layer and an instrumented re-run with scripted input:

- **68k RAM**: every address read/written by member code; split into *scratch* (written before read within the stage),
  *inputs* (read before written — e.g. emerald count, lives, rings), and *outputs* (written and read after exit).
- **VRAM / CRAM / VSRAM**: destination ranges written; VDP register values set (mode, plane sizes, scroll modes,
  window, H-int line).
- **Interrupts**: V-int / H-int behavior while in the stage (which handler paths run).
- **Sound**: sound IDs requested (music, SFX) and any direct driver/Z80 interaction.
- **Other hardware**: controller reads, Z80 bus requests, SRAM.

## 3. Contract

Built from function contracts (spec 11) plus the footprint, then reviewed:

- **Entry**: required state on entry (which RAM inputs, which emerald index is being attempted, rings).
- **Exit**: result (success/fail), emerald awarded, rings/score effects, game mode to return to.
- **Assumptions**: register conventions of shared routines, VDP state expected on entry, interrupt routing.

Contract claims that disagree with measured footprints are flagged; the splice can't be built while flags are open.

## 4. Splice plan (target: Sonic 2)

| Aspect | Approach |
|--------|----------|
| **Placement** | Donor module appended to the target source in a new section. If the ROM grows past 1 MB, the header ROM-end is updated and the build reports the size against the target cartridge (spec 16). |
| **Namespacing** | All donor symbols prefixed (`S1SS_`, `S3SS_`) to avoid collisions; local labels preserved. |
| **Raw references** | Any donor reference not expressed symbolically (relocation-risk report, spec 10) **blocks** the splice until resolved. |
| **RAM map** | Donor RAM addresses → target RAM. Default: map donor *scratch* into target RAM that is dead during special stages (the target's own special-stage and level RAM); *inputs/outputs* map to target equivalents via glue (e.g. donor emerald count ↔ S2 emerald count). The planner checks conflicts against target RAM that is live during special stages (sound driver comm, lives, score, emerald list) using target coverage. Conflicts → editable table. |
| **VRAM / VDP** | The stage owns the VDP while active. Glue saves nothing (the target reloads the level after) but must set the VDP state the donor expects on entry. |
| **Shared dependencies** | Each shared routine is either **mapped** to a target equivalent (an equivalence table: same algorithm + compatible register contract, e.g. Nemesis decompressor) or **copied** into the module with the prefix. Mapping requires contract compatibility; otherwise copy. |
| **Interrupts** | Glue hooks the donor's V-int/H-int behavior into the target's V-int dispatch (a new V-int routine slot for the spliced stage). |
| **Sound** | Donor sound IDs are translated: converted donor music/SFX added to the target's sound driver, or mapped to existing target IDs (Q17). Sonic 1 → Sonic 2 needs format conversion (68k SMPS → Z80 SMPS). |
| **Game-mode glue** | Generated assembly: entry shim (target state → donor inputs, VDP setup, jump to donor root), exit shim (donor outputs → target emerald award/rings, return game mode), stage selector table (Q13). |

The plan is stored in the workspace (spec 12) and generates source files into the target export tree; the
build is the normal modding pipeline (spec 14).

## 5. Validation

- **Static**: no unresolved donor references; RAM map conflict-free; all shared deps mapped or copied; contract flags
  closed; ROM size within target cartridge.
- **Splice equivalence harness**: play the stage in the donor game with scripted input and record per-frame
  subsystem RAM (owned + inputs/outputs), CRAM, VRAM hashes of owned ranges, and sound-ID requests. Run the spliced stage
  in the target with the same input; apply the RAM map; compare frame by frame. Report the first divergence with both
  traces. Expected differences (e.g. sound driver internals) are declared in an allowlist.
- **End-to-end**: scripted run in Sonic 2 enters the spliced stage from a special-stage ring, completes it, and the
  correct emerald is awarded; failure path returns to the level (and offers retry if that mod is enabled).
- **Hardware**: strict mode clean; flash-cart test (spec 14).

## 6. Tooling for S2 special-stage modifications

Viewers (and later simple editors) for Sonic 2 special-stage data: stage layout segments, object (ring/bomb)
placement, checkpoint ring requirements, and stage order — so native stages can be modified alongside spliced ones.
