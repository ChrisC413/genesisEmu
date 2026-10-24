# M08-07 · Sonic Retro reference label import

**Milestone:** [M08 – Disassembly & analysis](README.md)  
**Status:** Draft  
**Depends on:** M08-06  
**Spec refs:** [10-disassembler-and-analysis](../../docs/specs/10-disassembler-and-analysis.md#reference-label-import-d-004)

## Story

As a **reverse engineer**, I want labels from s1disasm/s2disasm/skdisasm imported as a separate, hideable symbol source, so that I can validate my analysis and optionally work with community names.

## Scope

- Import AS symbol/map output; record disasm commit hash
- SHA-1 alignment check
- Visible vs hidden (blind) mode
- Optional comment import

## Acceptance criteria

- [ ] Import for Sonic 1 succeeds when SHA-1 matches; refused otherwise
- [ ] Hidden mode removes reference names from all UI and LLM contexts

## Test plan

- `[rom]` test
