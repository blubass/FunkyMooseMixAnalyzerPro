# Funky Moose Mix Analyzer Plugin

This is the first native plugin companion for Funky Moose Mix Analyzer. It is a JUCE-based analyzer and optional master-assistant processor that can build as VST3, AU on macOS, and Standalone for quick local testing.

## What works now

- Audio is passed through unchanged unless Auto Master is explicitly enabled.
- Optional Auto Master applies conservative genre-aware loudness gain, low/presence/air tone shaping, stereo safety, adaptive glue compression, loudness-match preview metering, a release-score check, and a -1.0 dBTP output guard.
- Live Momentary, Short-Term, and Integrated LUFS are measured with K-weighting and gated integrated loudness.
- True Peak is measured with 4x oversampling.
- RMS, sample peak, crest factor, L/R peaks, mono-sum peak, mono RMS/loss, stereo correlation, M/S width, L/R balance, DC offset, and six-band balance are measured in real time.
- Spectral centroid, rolloff, resonance area, LRA, transient density, attack time, and percussive energy are analysed for tone and translation checks.
- Full-pass worst-case holds track peak, clipping, correlation, mono loss, low-mid buildup, and resonance, so final judgements are not based only on the last playback block.
- Per-band phase/correlation and side-energy are measured for frequency-specific stereo safety, including dedicated low-end mono safety checks.
- The full standalone genre profile list is available, including target LUFS, crest, low-end, presence, and correlation thresholds.
- A native editor shows Mix Score, confidence, verdict, target checks, safety/delivery preview, tone shape, Auto Master state, reference comparison, A/B snapshots, Instrumental mode, Reset, Clear Reference, Clear A/B, and Copy DAW notes.
- Host playback can automatically run a pass: playback start begins measurement and playback stop freezes the pass.
- Delivery preview estimates streaming, Apple, and broadcast normalization gain with resulting True Peak.
- Delivery risk is also considered in priority actions when normalization would push True Peak too high.
- LRA/macrodynamics are scored separately with genre-aware targets, so flat masters and overly jumpy sections are flagged.
- Low-end scoring distinguishes sub, bass, and low-mid buildup instead of judging only total low-end energy.
- Tone scoring distinguishes air, dullness, harsh presence, sibilance-zone resonance, and vocal presence masking.
- Reference comparison contributes its own action note when the current mix drifts meaningfully from the captured reference.
- Copy report now includes a Mix Doctor Summary, worst-case holds, Auto Master moves, gain reduction, loudness-match preview, release-score check, delivery preview, reference/A-B notes, and band-phase readouts.
- Copy JSON exports the same pass as a stable machine-readable report for desktop-app import, support notes, or session archiving.
- The top measured priority actions are severity-ranked and include measured values where useful, so hard blockers such as clipping, True Peak, mono loss, phase, translation, and confidence issues surface before polish moves.
- Captured references and A/B snapshots are stored in the plugin state so DAW projects can restore them with the session.

## Build

JUCE is required. On this machine a local JUCE checkout was found at:

```bash
/Users/uwearthurfelchle/Developer/JUCE
```

Configure and build directly:

```bash
cmake -S plugin -B plugin/build -DMIX_ANALYZER_JUCE_DIR=/Users/uwearthurfelchle/Developer/JUCE
cmake --build plugin/build --config Release
```

The current workspace path contains an ampersand, which can trip JUCE's VST3 manifest step in some shell commands. On macOS, prefer the wrapper script because it builds through a clean `/private/tmp` symlink and copies the resulting plugin bundles back into `plugin/artifacts`:

```bash
bash plugin/build_macos.sh
```

Expected outputs:

- `plugin/artifacts/Funky Moose Mix Analyzer.vst3`
- `plugin/artifacts/Funky Moose Mix Analyzer.component` on macOS
- `plugin/artifacts/Funky Moose Mix Analyzer.app`

## Analysis regression tests

The mix-judgement model has deterministic CTest coverage for profile sanity,
warm-up gating, ready full-pass judgement, clipping priority, low-end phase
blocking, and full-pass worst-case holds. The suite also generates small WAV
fixtures at runtime and runs them through the real processor path to verify
sample peak, RMS, crest, LUFS sanity, clipping, phase correlation, mono loss,
spectral band routing, Auto Master bypass/leveling/glue behaviour, and DAW state
round-trips for references/A-B snapshots and Auto Master parameters:

```bash
cmake -S plugin -B plugin/build -DMIX_ANALYZER_JUCE_DIR=/Users/uwearthurfelchle/Developer/JUCE
cmake --build plugin/build --config Release --target FunkyMooseMixAnalyzerAllTests
ctest --test-dir plugin/build --build-config Release --output-on-failure
```

## Host validation

Release builds run Tracktion pluginval at strictness level 5 before packaging.
Locally, download pluginval from the Tracktion/pluginval release page. VST3 can
be validated directly:

```bash
pluginval --strictness-level 5 "plugin/artifacts/Funky Moose Mix Analyzer.vst3"
```

AU validation has to go through macOS AudioComponent registration:

```bash
bash plugin/install_macos.sh --user
killall AudioComponentRegistrar || true
auval -v aufx FmMa Fnky
pluginval --strictness-level 5 "$HOME/Library/Audio/Plug-Ins/Components/Funky Moose Mix Analyzer.component"
```

## macOS release signing and notarization

The release workflow signs macOS bundles before packaging. Without Apple
credentials it falls back to ad-hoc signing, so test releases still work.

For Developer ID signing and Apple notarization, add these GitHub repository
secrets:

- `MACOS_DEVELOPER_ID_CERTIFICATE_BASE64`: base64-encoded `.p12` export of the
  Developer ID Application certificate and private key
- `MACOS_DEVELOPER_ID_CERTIFICATE_PASSWORD`: password for that `.p12`
- `APPLE_ID`: Apple developer account email used for notarization
- `APPLE_TEAM_ID`: Apple developer team ID
- `APPLE_APP_SPECIFIC_PASSWORD`: app-specific password for the Apple ID

The workflow also accepts `APPLE_DEVELOPER_ID_CERTIFICATE_BASE64` and
`APPLE_DEVELOPER_ID_CERTIFICATE_PASSWORD` as legacy aliases. When all secrets are
present, the macOS ZIP is submitted with `xcrun notarytool`, the standalone app
is stapled, VST3/AU stapling is attempted, and the final ZIP is recreated with
the stapled bundles.

## Install for Cubase / AU hosts

Cubase scans the standard VST3 folders, not this repository's `plugin/artifacts`
folder. Install the built bundles into the user plugin folders and force Cubase
to rebuild its VST3 scan cache:

```bash
bash plugin/install_macos.sh --user --reset-cubase-cache
```

For a system-wide install use:

```bash
sudo bash plugin/install_macos.sh --system --reset-cubase-cache
```

## Next plugin steps

- Add optional Windows Authenticode signing when a Windows code-signing certificate is available.
- Add a desktop-app import screen for the plugin JSON report.
