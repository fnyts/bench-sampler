# Bench Sampler

Bench Sampler is a desktop sample-prep workbench for building clean WAV folders for hardware samplers.

It is designed around the Teenage Engineering EP-133 K.O. II workflow, but it does not depend on that device. The exported files are normal WAV files in a simple four-bucket folder structure, so they can also be used with other samplers, DAWs, drum machines, or sample-pack workflows.

## What It Does

- Imports audio files into four sampler-style buckets:
  - `A_DRUMS`
  - `B_BASS`
  - `C_MELODY`
  - `D_OTHER`
- Lets you classify a sound as a one-shot, loop, or texture.
- Stores musical BPM, bars, key, capture markers, FX settings, and kept bounces per sample.
- Shows a waveform overview with bar-based capture regions.
- Previews the source or the rendered bounce from the local waveform transport.
- Renders a bounce preview WAV from the selected capture region.
- Keeps or trashes bounce previews explicitly.
- Provides built-in FX and first-pass VST3 hosting.
- Saves and opens editable `.sfbpack` session files.
- Exports a complete pack folder with bucket subfolders and notes.

## EP-133 K.O. II Scope

Bench Sampler is useful for preparing material before manual transfer to the EP-133 K.O. II.

It does not:

- upload files to the EP-133
- control the EP-133
- use or reverse-engineer sampler protocols
- replace the official Teenage Engineering transfer tools

The intended flow is:

1. Import sounds.
2. Set BPM and bar capture.
3. Add built-in FX or VST3 processing.
4. Render a bounce preview.
5. Listen to the rendered WAV.
6. Keep the good variations.
7. Export a bucketed WAV folder for manual transfer or further use.

## Export Format

Export Pack creates a folder like:

```text
PACK_NAME/
  A_DRUMS/
  B_BASS/
  C_MELODY/
  D_OTHER/
  PACK_NAME_notes.txt
```

Only kept bounces are exported by default. Temporary render previews are not part of the exported pack until they are kept.

Loop filenames keep musical BPM last:

```text
amen_wet_v2_092BPM.wav
amen_wet_v3_spdup_092BPM.wav
kick_dry_v1.wav
```

## Project Boundaries

Bench Sampler is not a DAW.

It intentionally avoids:

- timelines
- multitrack arrangement
- piano rolls
- mixer buses
- cloud sync
- sampler upload/control

The app is focused on one job: prepare, process, bounce, name, and export samples.

## Build

```sh
cmake -S . -B work/build-app
cmake --build work/build-app --target SamplerFoodBench
ctest --test-dir work/build-app --output-on-failure
```

To build only the portable model/tests without the JUCE app:

```sh
cmake -S . -B work/build-tests -DSAMPLE_BENCH_BUILD_APP=OFF
cmake --build work/build-tests
ctest --test-dir work/build-tests --output-on-failure
```

If JUCE is not installed locally, CMake fetches JUCE 8.0.6.

## Code Layout

- `src/model/`: portable pack/session/export/DSP model code
- `src/app/`: JUCE desktop app, waveform view, plugin scanner, and bench UI
- `tests/`: model, render, and palette tests

## Notes

This is a prototype, but the app keeps a hard boundary between previewing and exporting:

- Source preview is for auditioning.
- Render Preview creates a real temporary WAV.
- Keep Bounce commits that WAV as an exportable variation.
- Export Pack only uses kept variations.

That rule is intentional: the rendered bounce is the truth.
