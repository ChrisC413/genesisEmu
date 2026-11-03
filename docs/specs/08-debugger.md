# 08 · Debugger and Freeze Execution

## Run control

States: `Running`, `Paused`, `Stepping`. Stops always land on an **instruction boundary** of the focused CPU.

| Command | Behavior |
|---------|----------|
| Pause / Resume | Stop at the next instruction boundary of the 68k (or Z80 if it is the focused CPU) |
| Step into | Execute exactly one instruction on the focused CPU; the other CPU and chips advance by the corresponding MCLK |
| Step over | For `JSR`/`BSR`/`TRAP` (68k) and `CALL`/`RST` (Z80): run until return to the next instruction at the same stack depth |
| Step out | Run until the current subroutine returns (`RTS`/`RTE`/`RTR` / `RET`/`RETI` with SP above entry SP) |
| Run to cursor | Temporary exec breakpoint |
| Step scanline / Step frame | Run to the next line / frame boundary |
| Reset (soft/hard) | Soft = 68k reset line; hard = power cycle |

Implementation (M06-01/02, `gen::debug::Debugger` in `src/debug/`):

- The debugger is the only owner of the Machine hooks and installs just the callbacks that the active
  features need; with nothing active the hooks are null and the core runs exactly as without a debugger
  (D-030). Observing never changes results: a unit test runs with breakpoints, watchpoints, the ring buffer
  and call tracking on, and stops / resumes at 68k breakpoints, watchpoints and interrupts, and compares the
  state and frame hashes with an uninterrupted run.
- Resuming (run, step) never stops on the instruction the CPU is paused at. Step over / out / run to cursor
  are cancelled when a breakpoint or watchpoint stops first.
- Step over keys on the stack pointer: it stops at the instruction after the call with SP at or above its
  value at the call, so interrupts taken inside (and recursion) are stepped over too. Step out stops after the
  first return (`RTS`/`RTE`/`RTR`, `RET`/`RET cc`/`RETI`/`RETN`) that leaves SP above its value at the start.
- Z80 steps (D-031): the Z80 runs behind the 68k and catches up at 68k accesses and line ends. While it is
  behind, one Z80 instruction runs directly (the same as free running); while it is ahead, the 68k first runs
  with the Z80 held at its instruction until it is behind again. A Z80 breakpoint or watchpoint freezes the
  Z80 at its next instruction and stops the 68k at the end of its current one.
- App: Emulation menu, the Debugger panel and hotkeys (F5 pause/run, F11 step into, F10 over, Shift+F11 out,
  F6 frame, Shift+F6 scanline); a focused-CPU selector; a status bar with run state, frame, line, MCLK and
  the last stop. `genesisEmu --debugger [--pause-at=<frame>] [--break=<hex>] [--state=<file>] <rom>` opens the
  debugger layout.

## Breakpoints

| Type | Parameters |
|------|------------|
| Execute | CPU, address (or range), optional condition, hit count, enabled, temp |
| Memory read / write | CPU (or any), address range, size filter, optional value condition (`== / & mask`) |
| VDP | Register write (reg number), DMA start (by destination type), CRAM/VRAM/VSRAM write in range |
| Interrupt / exception | 68k level, exception vector number, Z80 INT |
| Frame | Break at frame N, or at V-int entry |

- **Conditions**: a small expression language over registers, memory and frame counter, e.g. `d0.w == $10 && byte($FFB000) != 0`. Parsed once, evaluated as bytecode.
- **Logpoints**: a breakpoint with "log, don't stop" and a format string (`"obj {a0:x} routine {byte(a0+$24)}"`).
- Exec breakpoints use a per-address bitmap for O(1) checks; memory watchpoints use a page bitmap + range list.

Implementation notes (M06-03/04):

- Expression language (`src/debug/expr.hpp`): literals `$1F`, `0x1F`, `%1010`, `31`; 68k registers `d0-d7`,
  `a0-a7`, `sp`, `pc`, `sr`, `ccr`, `usp`, `ssp`; Z80 registers `a f b c d e h l i r af bc de hl ix iy sp pc
  af' bc' de' hl'`; `.b`/`.w`/`.l` keep the low 8/16/32 bits; `byte(x)`, `word(x)`, `long(x)` read memory
  of the breakpoint's CPU without side effects; variables `frame`, `line`, `value` and `addr` (the access of a
  watchpoint) and `hits`; C operators and precedence. Logpoint fields are `{expr}` or `{expr:x4}` (`x`, `X`,
  `d`, `c`).
- A hit count skips the first N matching hits; temporary breakpoints are removed when they stop; a logpoint
  can also stop; any breakpoint can begin a trace of N frames when it hits.
- Memory watchpoints see the CPU's bus accesses: the 68k bus is 16 bits wide, so a long access is two word
  accesses. 68k work RAM mirrors (`$E00000-$FFFFFF`) are folded to `$FFxxxx`. A watchpoint stops after the
  accessing instruction and reports its PC, the address and the value.
- VDP breakpoints: register N written; VRAM / CRAM / VSRAM written in a range (data port or 68k -> VDP DMA,
  `onVdpWrite`); DMA start by target and destination (`onDma`). Interrupt breakpoints (by level, 68k or Z80)
  and exception breakpoints (by vector, `onException`) stop at the handler's first instruction; frame
  breakpoints at the first instruction of the frame.
- 100 exec breakpoints that never hit cost ~0.7 ms per frame in Sonic 1 (the per-instruction hook), well
  inside the spec 01 budget.

## Inspectors (panels)

| Panel | Contents |
|-------|----------|
| 68k registers | D0–D7, A0–A7, USP/SSP, PC, SR with flag checkboxes; editable when paused |
| Z80 registers | Main + shadow sets, IX/IY, SP, PC, I, R, IFF1/2, IM, halted, bus/reset state |
| Disassembly | Live listing around PC with symbols, breakpoints gutter, follow-PC toggle; uses the M08 symbol DB when available |
| Memory | Hex editor views for ROM, 68k RAM, Z80 RAM, VRAM, CRAM, VSRAM, SRAM; go-to, search bytes, edit when paused, highlight recent writes |
| Call stack | Heuristic 68k stack walk from `JSR`/`BSR` return addresses (validated against known call sites) |
| VDP registers | Decoded fields of all 24 registers and status |
| Tiles | VRAM as a tile sheet with palette selector; hover shows tile index and VRAM address |
| Planes | Plane A/B/window as full nametable images with scroll viewport overlay |
| Sprites | Decoded sprite list (link order, position, size, tile, palette, priority) + on-screen boxes |
| Palette | 4×16 CRAM swatches; per-line CRAM changes this frame |
| Audio | Channel mute/solo, register-write log (spec 06) |
| Breakpoints | List/enable/edit all breakpoints and logpoints |

## Save states

- **Slots** 1–9 per ROM plus unlimited named states. Each state stores: format version, ROM SHA-1, frame number, timestamp, a 160×112 thumbnail, and every component's chunk (spec 01).
- Save/load are only performed at instruction boundaries (always true when paused; when running, at the next frame end).
- A state from a different ROM SHA-1 is refused. A state from an older format version is loaded if all its chunks are understood; otherwise a clear error.
- Coverage and analysis data are **not** part of save states (they live in the project, spec 12). Loading a state does not erase coverage.
- Determinism check: save → run N frames → hash; load → run N frames → hash must match.
- Format (M06-00a, D-028): `GENESTATE` magic, a container version, then chunks `tag (4 ASCII), u16 version,
  u16 flags, u32 length, payload`, little-endian. `INFO` holds the ROM SHA-1, frame, timestamp and the
  `Machine::stateHash()` at save time; then one chunk per component (`M68K`, `BUS `, `VDP `, `IO  `, `Z80 `,
  `Z80C`, `SND `, `CART`, `SCHD`). Loading validates every chunk first, applies them, and refuses the state
  (rolling the machine back) unless the restored machine reproduces the recorded state hash. A state from
  another region is refused like one from another ROM.
- Thumbnails (M06-08): an optional `THMB` chunk (u16 width, u16 height, RGB888; 160x112) that the app and
  `gen_cli --save-state-at` write. It is not machine state: loading ignores it and a state without one is
  complete. The Save States panel lists slots 1-9 and named states (`<user data>/roms/<sha1>/states/*.gst`)
  with their thumbnails; File > Save State As / Load State File use a file dialog.
- Command line: `genesisEmu --state=<file> <rom>`; `gen_cli run|trace --load-state <file>` and
  `gen_cli run --save-state-at <frame>:<file>` for scripted runs that start right before a level.

## Tracing (trace-from-pause, D-007)

Typical flow: **pause → configure → begin trace → resume/step → stop trace**.

- Configuration: CPU(s), optional address-range filter (only log instructions whose PC is in range), optional "only first execution of each PC" mode, max size.
- Each record: frame, scanline, MCLK, CPU, PC, opcode bytes, disassembly text, register deltas (only registers that changed), memory writes made by the instruction.
- Output: text (human-diffable; one line per instruction) and a compact binary format (for tooling). Writing is buffered off the hot path.
- **Ring-buffer mode**: capture of the last N (default 100,000) instructions of both CPUs in memory; dumpable after a breakpoint hits — answers "how did we get here?". It is switched on in the Trace panel rather than always on (D-030): capturing costs a hook call per instruction.
- Traces can be started from a breakpoint action ("on hit: begin trace for 2 frames").
- Text trace format is stable and documented so traces from two runs can be diffed with standard tools.

### Text trace format (v1)

Written by `gen::instrument::TraceWriter` (`gen_cli trace`, M02-06). The file starts with the line
`# genesisEmu trace v1`; other lines starting with `#` are comments. Every other line is one record, in
execution order, with LF line endings:

```
<frame> <line> <mclk> <cpu> <pc>  <words> <disassembly> | <register deltas> | <memory writes>
0   0        280 M68K 000200  46FC 2700               move.w #$2700,sr                   | - | -
0 224     766080 M68K 0002A4                          interrupt level 6                  | A7=00FFFDFA SR=2600 | W.W FFFDFC=0000 W.W FFFDFE=02A4 W.W FFFDFA=2000
```

| Field | Format |
|-------|--------|
| frame | decimal frame number (0 = first frame after reset) |
| line | scanline the instruction started on, decimal, width 3 |
| mclk | master clock at the start of the instruction, decimal, width 10 |
| cpu | `M68K` (`Z80` from M04) |
| pc | 6 hex digits, upper case |
| words | opcode and extension words, 4 hex digits each, padded to 24 columns; empty for an interrupt entry |
| disassembly | AS syntax from the M01-12 disassembler with tabs replaced by spaces, padded to 34 columns; `interrupt level N` for an interrupt entry (its record covers the stacking and vector fetch, and its PC is the interrupted PC) |
| register deltas | registers whose value changed, in the order `D0-D7`, `A0-A7` (A7 = active stack pointer), `USP`/`SSP` (the inactive one), `SR`, as `NAME=hex`; `-` if none |
| memory writes | bus writes in order as `W.<B/W> <addr>=<value>` (a long write appears as two word writes); at most 16, then `+N`; `-` if none |

Filters: frame range (`--start-frame`, `--frames`), PC range (`--pc lo-hi`; interrupt entries are then
omitted), first execution only (`--first`) and a record limit (`--max-records`).

`tools/trace-diff.py ours.txt reference.txt` compares two traces record by record and prints the first
divergence with the preceding records. It ignores the frame/line/mclk columns unless `--timing` is given;
`--no-writes` and `--no-disasm` relax the comparison further. A reference emulator's trace has to be
converted to this format first.
