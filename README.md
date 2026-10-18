# KITSUNE Auto Tune

Real-time monophonic vocal pitch correction VST3 for Windows (JUCE 8 + CMake). Visual style: dark purple / pink "synthwave" panel.

> **Honest status:** the DSP core (`Source/DSP`) is compiled and unit-tested (see below). The JUCE plugin layer, UI, installer and CI
> were written carefully but **have not been compiled yet** in the authoring environment (no JUCE download / MSVC available).
> Expect to fix a few compile errors on your first Windows build — send them back and they get fixed.

## Build (Windows 10/11, clean machine)
1. Install: Visual Studio 2022 (workload "Desktop development with C++"), CMake >= 3.22, Git, Inno Setup 6 (for the installer).
2. ```
   git clone <your repo url> KITSUNE-Auto-Tune && cd KITSUNE-Auto-Tune
   cmake --preset vs2022            # downloads JUCE 8.0.6 via FetchContent
   cmake --build --preset release --parallel
   ctest --preset release
   ```
   Output: `build\KitsuneAutoTune_artefacts\Release\VST3\KITSUNE Auto Tune.vst3`
3. Installer: `powershell -ExecutionPolicy Bypass -File Installer\build-installer.ps1 -Version 0.1.0`
   -> `Installer\Output\KITSUNE-Auto-Tune-0.1.0-Setup.exe` (installs to `C:\Program Files\Common Files\VST3\KITSUNE Auto Tune.vst3`).
4. DSP tests only (any OS, no JUCE): `cmake --preset dsp-only && cmake --build --preset dsp-only && ctest --preset dsp-only`

## Publish to GitHub
```
git init && git add . && git commit -m "KITSUNE Auto Tune 0.1.0"
git branch -M main
git remote add origin https://github.com/<you>/KITSUNE-Auto-Tune.git
git push -u origin main
git tag v0.1.0 && git push origin v0.1.0     # triggers build + GitHub Release with installer
```

## Architecture
| Stage | File | Method |
|---|---|---|
| Pitch detection | `DSP/PitchDetector` | YIN on a ~12 kHz decimated signal, parabolic interpolation, octave-jump guard (3-frame confirmation), silence gate |
| Correction logic | `DSP/PitchCorrector` | Scale/note-mask quantiser with hysteresis, retune speed, strength, humanize, vibrato keep, hold/decay on unvoiced |
| Pitch shifting | `DSP/PitchShifter` | Pseudo-PSOLA: pitch-mark tracking + Hann grains re-spaced (timing and formants preserved by construction) |
| Formant control | inside `PitchShifter` | Grain read-rate (formant shift ±12 st, preserve toggle). Not a spectral-envelope vocoder |
| Tone | `DSP/ToneProcessor` | HPF + low shelf + mid bell + high shelf |
| Harsh reduction | `DSP/DynamicResonanceReducer` | 10 bands 1.5–11 kHz; band cut only when above the mean band level by a sensitivity margin |
| Glue | `DSP/PitchEngine`, `PluginProcessor` | Fixed-latency dry path, mono/stereo, smoothed gains/mix |

All DSP allocates only in `prepare()`; no locks/IO in `processBlock`. UI telemetry uses lock-free atomics.

## Latency (measured by `Tests/DSPTests.cpp`, algorithmic only — add your host buffer)
| Mode | Lowest pitch | Latency |
|---|---|---|
| Low Latency | 100 Hz | ~26.1 ms (1254 samples @48k) |
| Studio Quality | 65 Hz | ~40.2 ms (1928 samples @48k) |

Latency is reported to the host via `setLatencySamples`. It is **not** zero or "ultra low": a pitch-synchronous shifter needs about
2.5 pitch periods of the lowest supported note. Voices below the mode's lowest pitch are passed through uncorrected.
Switching modes resets the DSP (short glitch).

## Feature status
| Feature | Status |
|---|---|
| Real-time detection + correction, 14 scales, key, note on/off (custom/exclude), ref A4 | **Implemented, DSP unit-tested** |
| Retune speed, strength, humanize, vibrato keep | **Implemented** (tuned by ear still needed) |
| Formant shift / preserve | **Partial** (grain read-rate method; range limited for low voices in Low Latency mode) |
| Low Latency / Studio modes + reported latency | **Implemented** (see numbers above) |
| HPF, 3-band tone, dynamic harsh reduction (sens/amount/attack/release) | **Implemented** (EQ coefficients update per block; no spectrum analyser) |
| Presets (10 factory + user XML), state restore, A/B, undo/redo, bypass | **Implemented, JUCE layer not compiled yet** |
| Real-time pitch display, meters, reduction display | **Implemented, not compiled yet** |
| Spectrum visualisation | **Not implemented** |
| Melodyne-style pitch editor (piano roll, offline analysis/resynthesis, note editing) | **Not implemented** (planned Milestone 4) |
| Parametric EQ with free bands | **Not implemented** (fixed 3-band tone) |
| "Throat" character control | **Not implemented** |
| Installer EXE / VST3 binary | **Not built here** — scripts provided |

## Licensing
JUCE is dual-licensed **AGPLv3 / commercial**. Distributing this plugin as a closed-source product requires a JUCE commercial license;
otherwise the plugin must be released under AGPLv3 with source. Put the full AGPL-3.0 text from https://www.gnu.org/licenses/agpl-3.0.txt
into `LICENSE` (it is intentionally not faked here). Dependencies: JUCE 8.0.6 (AGPLv3/commercial), Inno Setup (installer tool, not shipped).
The fox logo is a simple original vector drawn in code; replace with your own artwork if desired.

## Docs
`docs/FL_STUDIO_TEST_CHECKLIST.md`
