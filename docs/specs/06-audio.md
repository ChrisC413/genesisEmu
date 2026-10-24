# 06 · Audio

## Chips

### YM2612 (FM)
- 6 FM channels × 4 operators, 8 algorithms, feedback, envelope (AR/DR/SR/RR/SL/TL), key scaling, detune, multiple.
- LFO (AM/PM), channel 3 special mode (per-operator frequencies), **SSG-EG**.
- **DAC**: register `$2A` data, `$2B` bit 7 enable — replaces channel 6. Used heavily by all Sonic games (drums, "SEGA" voice).
- Timers A/B with overflow flags; busy flag on status read.
- Clocked at MCLK/7, native output ≈ 53,267 Hz.
- Implemented with **ymfm** (`ymfm::ym2612`) behind an `IFmSynth` interface (D-013). The interface keeps ymfm out of save-state and bus code: `write(port, value)`, `read_status()`, `generate(samples)`, `serialize/deserialize`.

### SN76489 (PSG)
- 3 square tone channels + 1 noise channel (white/periodic, 16-bit LFSR with Sega's tap pattern), 4-bit attenuation (2 dB steps).
- Written by the 68k at `$C00011` or the Z80 at `$7F11`. In-house implementation.

## Mixing and output

- Both chips are advanced in catch-up fashion to the current MCLK whenever they are written, and at end of scanline.
- Resample to the host rate (48 kHz default) with a band-limited resampler (windowed sinc or polyphase); apply the Genesis-style low-pass (≈ 3.4 kHz first-order on model 1, configurable "none").
- Mix levels: FM vs PSG balance configurable; default matches a model 1 VA6 console approximately.
- Output via `SDL_AudioStream`. **Dynamic rate control**: adjust the resampling ratio by up to ±0.5% to keep the stream fill near a target (e.g. 2 × frame of audio), preventing drift between 59.92 Hz emulated video and the host's audio clock.
- When emulation is paused, output silence (no buzzing of the last buffer). When fast-forwarding, mute or pitch-preserve (mute is fine).

## Analysis features

| ID | Requirement |
|----|-------------|
| A-1 | Per-channel mute/solo (FM 1–6, DAC, PSG 1–3, noise). |
| A-2 | **Register write log**: ring buffer of `(frame, line, cpu, pc, chip, port, reg, value)` — for reverse engineering SMPS drivers. Exportable as CSV. |
| A-3 | Per-channel oscilloscope / level meters in the UI (optional, M05 stretch). |
| A-4 | WAV recording of the mixed output (used by regression tests). |

## Testing

- PSG: unit tests on tone period, noise LFSR sequence, attenuation table.
- FM: compare rendered WAVs of short known register sequences against reference captures (tolerance-based, not bit-exact, since the reference is a different implementation).
- Regression: hash of audio output for a scripted-input run of each game's title screen, with a stored golden hash per implementation version.
