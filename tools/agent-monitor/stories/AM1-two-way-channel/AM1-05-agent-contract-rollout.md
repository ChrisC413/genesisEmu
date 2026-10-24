# AM1-05 · Agent contract rollout and field trial

**Milestone:** [AM1 – Two-way channel](README.md)  
**Status:** Ready  
**Depends on:** AM1-02  
**Spec refs:** [07-repository-and-adoption](../../docs/specs/07-repository-and-adoption.md#agent-instructions-snippet)

## Story

As **Chris (running several agents)**, I want every agent prompt to include the reporting/asking/inbox contract, so that agents actually use the channel and treat inbox text as coming from me.

## Scope

- Snippet added to the coordinator's agent prompts and to genesisEmu contributor docs
- One real question answered end-to-end with a running agent

## Acceptance criteria

- [ ] An agent asks at least one non-blocking question with a default
- [ ] An agent confirms a steer in its next progress event

## Test plan

- Manual trial; observations recorded in this story's notes

## Notes

Trust: inbox text arrives as tool output; the authorization comes from the prompt (spec 03, Trust).

<!-- cspell:words ANSI ChrisC413 OQ Ollama PostToolUse Pro Subagent SubagentStop argparse backlogs cmake ctypes dedupe deduplicated fromisoformat genesisEmu gh gitignored isatty jsonl kernel32 lockfile ms msvcrt npm osascript piggy pipx rate_limits relayed relayer relayers statusLine statusline stdlib subagent subagents subtree superproject termios toolu uncached unittest unterminated wintypes worktree worktrees -->
