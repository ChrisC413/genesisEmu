# 11 · LLM-Assisted Decompilation

Goal: turn a 68k assembly function into readable **pseudo-C** with meaningful names for the function, its
locals, the RAM variables and struct fields it touches — proposed by an LLM, reviewed and accepted by the user.

**Why (D-019):** the end goal is **splicing special stages** between games (spec 15). Pseudo-C and names
are the means; the deliverable that matters most for splicing is a reviewed **interface contract** for
each function and for each subsystem — what it reads, writes, calls, and assumes about hardware. The splice
itself is performed on assembly, not on the pseudo-C.

## Provider abstraction (D-006)

```cpp
struct LlmRequest  { std::string system; std::vector<Message> messages; JsonSchema outputSchema;
                     std::string model; int maxTokens; std::vector<CacheBreakpoint> cache; };
struct LlmResponse { nlohmann::json output; Usage usage; std::string rawText; std::string stopReason; };

class ILlmProvider {
public:
  virtual std::future<LlmResponse> complete(const LlmRequest&) = 0;
  virtual std::string id() const = 0;   // "anthropic", "local-ollama", ...
};
```

- **Default provider: Anthropic Claude API** (Messages API over HTTPS via libcurl). Default model:
  the latest Claude model (configurable per request type; e.g. a smaller, cheaper model for bulk naming
  passes and the most capable model for difficult functions).
- Structured output: responses must validate against a JSON schema; invalid responses are retried once with
  the validation error, then surfaced to the user.
- **Prompt caching**: the large, stable prefix (system prompt, hardware reference, global symbol table) is
  marked cacheable so that per-function requests are cheap.
- API key: environment variable `ANTHROPIC_API_KEY` or the OS keychain (spec 07). Never written to logs,
  projects or traces.

## Context bundle

Built per function by `ContextBuilder`. Sending ROM-derived disassembly to the API is allowed (D-018); the
exact payload can be previewed in the UI but preview is not required before sending.

| Section | Content |
|---------|---------|
| Hardware reference | Concise Genesis register/memory-map cheat sheet (static, cached) |
| Game conventions | Known structs (SST layout), calling conventions observed (e.g. `a0` = current object), common macros |
| Target function | Disassembly with current labels and comments, typed data it references |
| Callees | Names + one-line summaries (or signatures) of called functions; full code for small leaf callees (≤ 20 instrs) |
| Callers | Up to N call sites with a few instructions of context |
| RAM usage | Every RAM address/struct offset read or written, with current names, observed value ranges (from coverage/watch data) |
| Runtime facts | Exec counts, "every frame" flag, watch-session membership, observed jump-table indices |
| Constraints | Names must be `PascalCase` for functions, `snake_case` for variables/fields; never reuse a name already bound elsewhere unless proposing a merge |

In **blind mode** (scoring, spec 10) reference-sourced labels are replaced by auto labels before building
the bundle.

## Output schema (abridged)

```json
{
  "function_name": "Sonic_Jump",
  "summary": "Starts a jump when A/B/C is newly pressed and there is headroom.",
  "pseudocode": "void Sonic_Jump(Object *obj) { ... }",
  "line_map": [{"pseudo_line": 3, "asm_addresses": ["0x01A2C4", "0x01A2C8"]}],
  "parameters": [{"register": "a0", "name": "obj", "type": "Object*"}],
  "symbol_proposals": [
    {"address": "0xFFF602", "kind": "ram", "name": "ctrl1_pressed", "confidence": 0.8, "evidence": "..."},
    {"address": "0x01A2D0", "kind": "local_label", "name": ".no_jump", "confidence": 0.9}
  ],
  "struct_field_proposals": [{"struct": "Object", "offset": "0x10", "name": "x_vel", "confidence": 0.7}],
  "questions": ["Is $FFF7C8 a timer or a flag?"],
  "confidence": 0.75
}
```

## Interface contracts (for splicing)

A second request type, `contract`, asks for a structured contract instead of (or in addition to) pseudo-C.
For a function, the LLM's draft is **cross-checked against facts the tools already know** — static xrefs
and runtime coverage — and every claim that disagrees with them is flagged rather than accepted.

```json
{
  "function": "SS_Sonic_Move",
  "inputs":  [{"kind": "ram", "symbol": "ctrl1_held", "address": "0xFFF604", "size": 1}],
  "outputs": [{"kind": "ram", "symbol": "ss_sonic_x_vel", "address": "0xFFB010", "size": 2}],
  "calls": ["SS_CalcAngle"],
  "hardware": [{"kind": "vdp_reg", "reg": 11, "note": "sets line H-scroll"}],
  "vram_ranges": [], "sound_ids": ["sfx_jump"],
  "assumptions": ["a0 = player SST", "called once per frame from SS main loop"],
  "re_entrancy": "not re-entrant; uses scratch RAM $FFFF8000–$FFFF8040"
}
```

Subsystem-level contracts (spec 15) are assembled from function contracts plus measured footprints; the LLM
then writes the human-readable summary and proposes the entry/exit glue description.

## Workflow

1. User selects a function (listing, ROM map, or "next unnamed hot function" queue).
2. Preview context → send. Response appears in a side-by-side **asm ↔ pseudo-C** view; clicking a pseudo-C line highlights its asm addresses via `line_map`.
3. **Review**: each symbol proposal is shown as a diff (old → new, confidence, evidence) with accept / edit / reject. Accepted names enter the symbol DB with source `llm` and a link to the response.
4. Accepting names can trigger re-analysis of dependent functions (callers get better context next time).
5. **Batch mode**: process a queue (e.g. all "every frame" functions bottom-up from leaves) with a per-run cost cap, concurrency limit and pause/resume. Results land as *pending proposals*, never auto-accepted unless the user enables auto-accept above a confidence threshold.

## Caching and provenance

- Responses cached by `(function bytes hash, context bundle hash, model, prompt version)`; re-requests are free.
- Every stored result records: model, prompt version, token usage, cost estimate, timestamp.
- Prompt templates are versioned files in the repo (`src/analysis/llm/prompts/`), so changes are reviewable.

## Evaluation

- **Blind scoring** against reference labels: run blind on a sample, then compare proposed names with hidden reference names: exact match, normalized match (case/underscores/abbreviations), and semantic equivalence judged by a separate LLM call with a fixed rubric.
- Track: naming score, pseudo-code acceptance rate (user-rated), cost per function.
- Success target in spec 00 (≥ 60% on a 50-function sample).

## Limits

The LLM proposes; it never edits the ROM, the export, or accepted symbols on its own. Pseudo-C is
documentation, not compilable recompiler output. Contracts the LLM drafts become authoritative only after
tool cross-checks pass and the user accepts them.
