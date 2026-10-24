# M09 · LLM-assisted decompilation

**Goal:** Assembly → pseudo-C, proposed names and interface contracts via the Claude API behind a pluggable provider, with context building, review workflow, batching, caching and blind evaluation — aimed at making special stages transplantable (D-019).

**Depends on:** M08

## Testable stage (exit criteria)

Select any Sonic 1 function → receive pseudo-C linked line-by-line to asm and a list of name proposals → accept some → names appear everywhere. Request a contract for a special-stage function → the tool cross-checks it against xrefs and coverage and flags disagreements. Blind evaluation on 50 functions reports ≥ 60% naming agreement with reference labels.

## Notes

Sending ROM-derived disassembly to the API is allowed (D-018). Prioritize special-stage code of Sonic 1, Sonic 2 and S3K when choosing what to decompile first.

## Stories

| ID | Story | Depends on |
|----|-------|------------|
| M09-01 | [LLM provider interface and Claude provider](M09-01-llm-provider-and-claude.md) | — |
| M09-02 | [Context bundle builder](M09-02-context-bundle-builder.md) | M09-01 |
| M09-03 | [Decompile a function](M09-03-decompile-function.md) | M09-02 |
| M09-04 | [Side-by-side asm ↔ pseudo-C view](M09-04-asm-pseudocode-view.md) | M09-03, M08-08 |
| M09-05 | [Naming proposal review](M09-05-naming-proposal-review.md) | M09-03, M08-06 |
| M09-06 | [Batch decompilation and cost control](M09-06-batch-and-cost-control.md) | M09-05 |
| M09-07 | [Result cache and provenance](M09-07-result-cache-and-provenance.md) | M09-03 |
| M09-08 | [Blind evaluation against reference labels](M09-08-blind-evaluation.md) | M09-06, M08-07 |
| M09-09 | [Function interface contracts with cross-checks](M09-09-interface-contracts.md) | M09-03, M09-05 |
