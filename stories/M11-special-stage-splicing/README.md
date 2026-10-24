# M11 · Special-stage splicing

**Goal:** Extract a special stage from a donor game (Sonic 1 first, then S3K Blue Sphere) with everything it depends on, and transplant it into Sonic 2.

**Depends on:** M10, M09 (contracts), M07 (watch sessions/footprints)

## Testable stage (exit criteria)

In Sonic 2, entering a special-stage ring launches the Sonic 1 special stage (per the stage table); completing it awards the correct Sonic 2 emerald, failing it returns to the level (or offers retry). The splice equivalence harness shows no unexpected divergence from the stage running in Sonic 1. Then the same for Blue Sphere.

## Notes

Q12 (donors), Q13 (stage selection) and Q17 (sound) shape this milestone.

## Stories

| ID | Story | Depends on |
|----|-------|------------|
| M11-01 | [Workspace with donor and target projects](M11-01-workspace-multi-project.md) | — |
| M11-02 | [Subsystem definition and closure](M11-02-subsystem-definition-and-closure.md) | M11-01 |
| M11-03 | [Resource footprint measurement](M11-03-resource-footprint.md) | M11-02 |
| M11-04 | [Subsystem contract](M11-04-subsystem-contract.md) | M11-03, M09-09 |
| M11-05 | [Shared-dependency equivalence mapping](M11-05-dependency-equivalence-mapping.md) | M11-04 |
| M11-06 | [Namespaced, relocatable module export](M11-06-namespaced-module-export.md) | M11-05 |
| M11-07 | [RAM and VRAM remapping planner](M11-07-ram-vram-remapping.md) | M11-03 |
| M11-08 | [Game-mode, interrupt and result glue generation](M11-08-glue-generation.md) | M11-06, M11-07 |
| M11-09 | [Donor sound porting](M11-09-sound-porting.md) | M11-03 |
| M11-10 | [Splice equivalence harness](M11-10-splice-equivalence-harness.md) | M11-08 |
| M11-11 | [First splice: Sonic 1 special stage into Sonic 2](M11-11-splice-s1-into-s2.md) | M11-09, M11-10 |
| M11-12 | [Second splice: S3K Blue Sphere into Sonic 2](M11-12-splice-blue-sphere-into-s2.md) | M11-11 |
