# AM1-06 · Answer and steer from the live view

**Milestone:** [AM1 – Two-way channel](README.md)  
**Status:** Draft  
**Depends on:** AM1-03  
**Spec refs:** [03-two-way-channel](../../docs/specs/03-two-way-channel.md#user-interface)

## Story

As **Chris (running several agents)**, I want to answer a question without leaving the live view, so that responding takes seconds.

## Scope

- Keys: `a` answer (pick question), `s` steer (pick agent), `n` note, `Esc` cancel
- Line editor at the bottom of the screen; Windows (`msvcrt`) and POSIX (`termios`) input

## Acceptance criteria

- [ ] Rendering keeps running while typing
- [ ] Cancel leaves no partial message

## Test plan

- Unit tests for the key-dispatch state machine; manual on both platforms

## Notes

—

<!-- cspell:words ANSI ChrisC413 OQ Ollama PostToolUse Pro Subagent SubagentStop argparse backlogs cmake ctypes dedupe deduplicated fromisoformat genesisEmu gh gitignored isatty jsonl kernel32 lockfile ms msvcrt npm osascript piggy pipx rate_limits relayed relayer relayers statusLine statusline stdlib subagent subagents subtree superproject termios toolu uncached unittest unterminated wintypes worktree worktrees -->
