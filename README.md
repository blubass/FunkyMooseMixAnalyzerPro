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
The native macOS app (.app / .dmg) is in the works. Until then, you can easily start the tool via the terminal.

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
