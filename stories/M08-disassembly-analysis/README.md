# M08 · Disassembly & analysis

**Goal:** The analysis toolset: project DB, code discovery seeded by coverage, jump tables, data typing, xrefs, symbols, reference-label import, listing UI, and bit-exact reassemblable export.

**Depends on:** M07 (coverage seeds), M01-12 / M04-06 (disassemblers)

## Testable stage (exit criteria)

For Sonic 1 (then 2 and 3K): exported AS source reassembles to a byte-identical ROM; code/data classification agrees with s1disasm for ≥ 99% of bytes; listing UI supports navigation, rename, comment and typing.

## Notes

M08-01 (project DB) is needed early by M07-10; build it first.

## Stories

| ID | Story | Depends on |
|----|-------|------------|
| M08-01 | [Project database](M08-01-project-database.md) | — |
| M08-02 | [Recursive-descent code discovery](M08-02-code-discovery.md) | M08-01, M07-01 |
| M08-03 | [Jump and pointer table detection](M08-03-jump-table-detection.md) | M08-02 |
| M08-04 | [Data typing and asset formats](M08-04-data-typing-and-formats.md) | M08-01 |
| M08-05 | [Cross-references and function boundaries](M08-05-cross-references-and-functions.md) | M08-02, M08-03 |
| M08-06 | [Symbols, comments and structs](M08-06-symbol-management.md) | M08-01 |
| M08-07 | [Sonic Retro reference label import](M08-07-reference-label-import.md) | M08-06 |
| M08-08 | [Analysis listing UI](M08-08-analysis-listing-ui.md) | M08-05, M08-06 |
| M08-09 | [Reassemblable AS export](M08-09-reassemblable-export.md) | M08-05, M08-04, M08-06 |
| M08-10 | [Round-trip verification](M08-10-round-trip-verification.md) | M08-09 |
| M08-11 | [Accuracy scoring against reference disassemblies](M08-11-accuracy-scoring.md) | M08-07, M08-05 |
