![Funky Moose Banner](assets/screenshots/banner.png)

<p align="center">
  <img src="https://img.shields.io/github/license/blubass/FunkyMooseMixAnalyzerPro?color=00f2fe&style=flat-square" alt="License">
  <img src="https://img.shields.io/badge/version-v0.9--beta-magenta?style=flat-square" alt="Version">
  <img src="https://img.shields.io/badge/platform-macOS%20|%20Windows-blueviolet?style=flat-square" alt="Platform">
</p>

# 🦌 Funky Moose Mix Analyzer Pro

[🇬🇧 English Version](#) | [🇩🇪 Deutsche Version](README_DE.md)

**A local mix analyzer for musicians who want to quickly understand what their master is actually doing.** 

No replacement for your ears – but a damn good second pair of eyes. The Funky Moose Mix Analyzer Pro is a tool for your home studio that checks your audio exports against proven genre standards. It gives you objective metrics on loudness, frequency balance, and phase correlation—without ever uploading your tracks to a cloud.

![Upload Screen](assets/screenshots/upload_screen.png)

---

## ✨ Moose Power

*   **Interactive FFT Analysis**: Dive deep into your frequency spectrum. Includes hover values, M/S representation, and target curves.
*   **Honest Track Comparison (A/B)**: Load reference tracks and compare them directly with your mix. The difference curve immediately shows you where you stand compared to your idol.
*   **Genre Reference Curves**: Over 30 profiles ranging from Techno to Rock to Podcasts help you find the right balance.
*   **Funky Moose Advice Engine**: Instead of empty AI buzzwords, you get solid tips based on your actual measurements – from "clean up the low end" to resonance warnings.

![Advice Engine](assets/screenshots/advice_engine.png)

*   **Loudness & Dynamics**: Measuring LUFS (EBU R128), True Peak, and Crest Factor for a competitive level.
*   **Privacy First**: Your music is sacred. All analyses run 100% locally on your machine.

---

## 🚀 Installation & Launch

### For Musicians & Producers
A macOS beta build can be generated locally. Public downloads will follow after testing. Until then, you can easily start the tool via the terminal.

### For Developers (and the Curious)
Make sure **FFmpeg** is installed:
*   **macOS**: `brew install ffmpeg`
*   **Windows**: `choco install ffmpeg`

1. Clone the repository:
   ```bash
   git clone https://github.com/blubass/FunkyMooseMixAnalyzerPro.git
   cd FunkyMooseMixAnalyzerPro
   ```

2. Create a virtual environment (recommended):
   ```bash
   python3 -m venv .venv
   source .venv/bin/activate  # macOS/Linux
   # or: .venv\Scripts\activate  # Windows
   ```

3. Install dependencies & Start:
   ```bash
   pip install -r requirements.txt
   python app.py
   ```

---

## 🧪 Test Run

To check the mathematical accuracy of the engine, you can run automated tests against the running backend.

1. Start the app (`python app.py`).
2. Drop your own test files into `tests/test_files/`:
   * `loud_master.wav`
   * `dynamic_track.wav`
   * `problematic_bass.wav`
3. Run the runner:
   ```bash
   bash tests/run_tests.sh
   ```

*Note: Test files are not included in the repository to keep the size small.*

---

## 🛠️ Plugin Development

The Funky Moose Mix Analyzer is also available as a JUCE-based audio plugin (VST3, AU, Standalone).

### Building the Plugin

1. **Prerequisites**:
   - CMake 3.16+
   - JUCE framework (cloned or installed)
   - C++17 compiler

2. **Clone JUCE** (if not already):
   ```bash
   git clone https://github.com/juce-framework/JUCE.git
   ```

3. **Build**:
   ```bash
   cd plugin
   mkdir build && cd build
   cmake .. -DMIX_ANALYZER_JUCE_DIR=/path/to/JUCE
   make -j$(nproc)
   ```

4. **Run Tests**:
   ```bash
   ctest --output-on-failure
   ```

### API Documentation

The plugin exposes the following metrics via `getMetrics()`:

- **Loudness**: momentaryLufs, shortTermLufs, integratedLufs
- **Dynamics**: truePeakDb, crestDb, lraLu
- **Stereo**: correlation, phaseCorrelation, msRatioDb
- **Spectrum**: spectralCentroidHz, resonanceFreqHz
- **Bands**: bandPercents, bandCorrelations
- **Auto Master**: optional off-by-default processing with strength, target LUFS, ceiling, gain, tone-shaping, stereo width, glue-compression, limiter-reduction, loudness-match A/B preview, release-score, and equal-loudness A/B quality readouts

For full API details, see `PluginProcessor.h`.

---

## 🏗️ Building the macOS App

You can generate the native macOS bundle (`.app`) and the installer (`.dmg`) yourself:

1. **Dependencies**:
   ```bash
   pip install pyinstaller
   ```
2. **Build App**:
   ```bash
   bash scripts/build_macos_app.sh
   ```
3. **Create DMG**:
   ```bash
   bash scripts/make_dmg.sh
   ```

---

## 🛠 The Tech behind the Antlers

*   **Backend**: Python & Flask
*   **Audio Engine**: NumPy & FFmpeg (Loudness & Decoding)
*   **Frontend**: PyWebView & Chart.js for interactive visualizations.
*   **Database**: SQLite for your local analysis history.

---

## 🔬 For the Nerds (Mathematics)
The analyzer uses Fast Fourier Transformation (FFT) with Hann windowing for precise frequency resolution. The onset detection is based on Spectral Flux with an adaptive noise floor to find precise transients even in heavily limited material ("sausage waveforms").

---

## 📄 License
This project is licensed under the **MIT License** – use it, improve it, make music with it.

*Developed with heart & moose blood by Uwe Arthur Felchle*
