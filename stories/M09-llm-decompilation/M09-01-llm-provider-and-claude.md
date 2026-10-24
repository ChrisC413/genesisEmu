# M09-01 · LLM provider interface and Claude provider

**Milestone:** [M09 – LLM-assisted decompilation](README.md)  
**Status:** Draft  
**Depends on:** —  
**Spec refs:** [11-llm-decompilation](../../docs/specs/11-llm-decompilation.md#provider-abstraction-d-006)

## Story

As a **developer**, I want an `ILlmProvider` interface with an Anthropic Claude implementation, so that decompilation can use the best model now and other providers later.

## Scope

- Messages API over libcurl; async with cancellation
- Structured JSON output validated against schema; one retry on failure
- Prompt-caching breakpoints
- Key from env/keychain; never logged
- Model configurable per request type; defaults to the latest Claude models
- Rate-limit/backoff handling

## Acceptance criteria

- [ ] Mock provider for tests
- [ ] Live smoke test (opt-in) returns schema-valid output
- [ ] Key never appears in logs or project files (test greps outputs)

## Test plan

- Unit tests with mock HTTP
- Opt-in live test
