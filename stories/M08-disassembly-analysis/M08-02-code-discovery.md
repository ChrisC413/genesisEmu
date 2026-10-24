# M08-02 · Recursive-descent code discovery

**Milestone:** [M08 – Disassembly & analysis](README.md)  
**Status:** Draft  
**Depends on:** M08-01, M07-01  
**Spec refs:** [10-disassembler-and-analysis](../../docs/specs/10-disassembler-and-analysis.md#code-discovery)

## Story

As a **reverse engineer**, I want code discovered from vectors and coverage via recursive descent with confidence levels, so that unexecuted but reachable code is found and data isn't misread as code.

## Scope

- Seeds: vectors, coverage exec starts, user entries
- Follow flow; stop rules; conflicts flagged
- Incremental re-runs never override user decisions
- Background job with progress

## Acceptance criteria

- [ ] Every executed instruction is in a code region
- [ ] No region conflicts silently resolved
- [ ] Full Sonic 1 analysis < 10 s

## Test plan

- Unit tests with synthetic ROMs
- `[rom]` scoring via M08-11
