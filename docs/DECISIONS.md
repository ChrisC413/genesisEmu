# Decision Log

Short-form ADRs. To change a decision, add a new entry that supersedes the old one; don't rewrite history.

| ID | Date | Decision | Rationale | Status |
|----|------|----------|-----------|--------|
| D-001 | 2026-10-23 | **C++20, CMake, SDL3, Dear ImGui (docking branch)** | Most emulator/debugger prior art; ImGui suits dense debug tooling; a future C recompiler can link against the same runtime. | Accepted |
| D-002 | 2026-10-23 | **Instruction-accurate timing**, scanline-based VDP | Each CPU instruction executes atomically with exact cycle counts; the VDP renders per scanline. Sufficient for all Sonic titles and far simpler to instrument than bus-cycle accuracy. | Accepted |
| D-003 | 2026-10-23 | **Targets: Sonic 1, Sonic 2, Sonic 3 & Knuckles** (cartridge only) | Sega CD and 32X are out of scope. S2 requires interlace mode 2; S3K requires SRAM and the lock-on ROM layout. | Accepted |
| D-004 | 2026-10-23 | **Use Sonic Retro disassemblies (s1disasm, s2disasm, skdisasm) for validation and label import** | Ground truth for scoring our disassembler and LLM naming; importable label sets. Imported labels are stored as a distinct symbol source so "blind" scoring stays possible. | Accepted |
| D-005 | 2026-10-23 | **Reassemblable ASM first, C recompilation later** | Bit-identical reassembly is a verifiable milestone and immediately enables ROM mods. C static recompilation is a later phase (M10 spike). | Accepted |
| D-006 | 2026-10-23 | **Claude API behind a pluggable LLM provider interface** | Best quality by default; the interface allows local models later. | Accepted |
| D-007 | 2026-10-23 | **Freeze features: pause, save states, trace-from-pause** | Chosen by the user. Rewind and input movies are out of scope (a minimal scripted-input format exists for automated tests only). | Accepted |
| D-008 | 2026-10-23 | **vcpkg (manifest mode) for third-party dependencies** | Reliable SDL3 / SQLite / curl builds on both MSVC and Apple clang. | Proposed |
| D-009 | 2026-10-23 | **SQLite per-ROM project database + text export** | Robust storage for symbols, coverage and LLM results; text export keeps analysis diffable in git. | Proposed |
| D-010 | 2026-10-23 | **Reassembly target: The Macro Assembler AS** | Open source, cross-platform, and used by s2disasm/skdisasm (s1disasm supports it too). | Proposed |
| D-011 | 2026-10-23 | **The core is a headless, deterministic library**, separate from the UI | Enables CI testing, reproducible analysis, and reuse by the future recompiler runtime. | Proposed |
| D-012 | 2026-10-23 | **Single emulation thread; UI on the main thread drives the core** | Debugger pause/step semantics stay trivial; the core runs a frame (or until a break) per UI tick. Audio is pushed into an SDL3 audio stream. | Proposed |
| D-013 | 2026-10-24 | **YM2612 via ymfm (BSD-3)** behind `IFmSynth` (Q1) | Hand-written requirement applies to the CPUs only; ymfm is accurate and permissively licensed. | Accepted |
| D-014 | 2026-10-24 | **MIT license** (Q2) | Only permissively licensed sources may be consulted or vendored; never copy from non-commercial-licensed emulators. | Accepted |
| D-015 | 2026-10-24 | **Platforms: Windows 11 x64 and macOS 13+** (Q3) | Apple Silicon native; x86_64 Mac best effort. | Accepted |
| D-016 | 2026-10-24 | **GitHub + GitHub Actions, free tier only; macOS validated locally** (Q4) | CI runs on free `windows-latest` runners only (no larger runners, no paid minutes). macOS builds and tests run via a local validation script before merging. | Accepted |
| D-017 | 2026-10-24 | **No widescreen.** The modding goals are: modify special stages, splice special stages from other Sonic games into Sonic 2, add retry-after-failure options, and support a Sonic 2 cartridge hardware mod (emerald LEDs) (Q6) | All mods stay within the original hardware model, so the reassembly path (D-005) is sufficient. C recompilation moves to the backlog. | Accepted |
| D-018 | 2026-10-24 | **Sending ROM-derived disassembly to the Claude API is allowed** (Q7) | The context preview stays available but is not a mandatory gate. | Accepted |
| D-019 | 2026-10-24 | **The purpose of LLM decompilation is enabling special-stage splicing** (Q8) | LLM output is pseudo-C for understanding plus a structured **interface contract** (entry/exit, RAM/VRAM/sound footprint, hardware assumptions). The splice itself is done at the assembly level. | Accepted |
| D-020 | 2026-10-24 | **NTSC first** (Q10) | PAL timing exists but is only lightly tested. | Accepted |
| D-021 | 2026-10-24 | **Execution tiers: once, more than once, per frame, more than per frame**; data categories include sprite, sound, graphics, palette and not used (Q11) | Matches how a reverse engineer reasons about Sonic code: init vs. event vs. per-frame logic vs. per-object/inner loops. | Accepted |
| D-022 | 2026-10-24 | **Cartridge hardware-mod register lives in the `/TIME` range (`$A130xx`)** | `/TIME` is the standard cartridge-side strobe for mapper registers (as used by `$A130F1`), so real hardware can decode it with a small CPLD/MCU. The exact address is fixed in spec 16. | Proposed |
| D-023 | 2026-10-24 | **Agent monitoring in Python 3 (standard library only)**, polling `.agent-status/*.jsonl`; terminal UI first ([spec 17](specs/17-agent-monitoring.md)) | Python is already installed on both platforms and the stdlib covers JSON, subprocess, threading, tests and Windows console/append via ctypes, so nothing is installed and nothing to lock. Node would need npm packages for a comparable TUI. Polling keeps Windows and macOS identical. | Proposed |
