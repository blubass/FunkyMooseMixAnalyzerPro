<div align="center">
  <img src="static/images/logo.png" alt="Funky Moose Mix Analyzer Pro Logo" width="250" />
</div>

# Funky Moose Mix Analyzer Pro

**Funky Moose Mix Analyzer Pro** is a production-ready, feature-rich desktop-style audio analysis application designed for quick mix checks, client-ready reports, and reference comparisons. 

The application utilizes Flask for the local API, PyWebView for the native window shell, ffmpeg/ffprobe for accurate audio decoding and loudness measurements, NumPy for advanced metric calculations, Matplotlib for rendering high-fidelity waveform and spectrum visualizations, and Chart.js for an interactive and dynamic dashboard.

## Highlights

- **Multi-slice analysis**: Analyze intro, middle, and outro sections independently.
- **30+ genre profiles**: Comprehensive profiles providing practical targets for LUFS, dynamics, low-end, presence, and stereo width.
- **Extensive Metrics**: Measure Loudness, true peak, sample peak, crest factor, stereo correlation, and frequency-band distribution.
- **Quality Assurance**: Automated quality checks for clipping, silence ratio, stereo balance, spectral centroid, and rolloff.
- **Bilingual Interface**: Full German and English UI support with localized insight data.
- **Reference Library**: Local SQLite-backed analysis history with A/B reference-track comparison engine.
- **Professional Exports**: Generate and export branded PDF reports for clients.
- **Offline Functionality**: Total offline capability with all external web assets (fonts, icons, libraries) fully localized and runtime-safe storage outside the packaged app bundle.


## Requirements

- Python 3.9+
- ffmpeg and ffprobe available in `PATH`
- Python dependencies from `requirements.txt`

On macOS with Homebrew:

```bash
brew install ffmpeg
python3 -m venv .venv
source .venv/bin/activate
python3 -m pip install -r requirements.txt
python3 app.py
```

## Runtime Data

Generated audio, charts, snippets, and `history.db` are stored outside the source tree by default:

- macOS: `~/Library/Application Support/Funky Moose Mix Analyzer`
- Windows: `%APPDATA%\Funky Moose Mix Analyzer`
- Linux: `$XDG_DATA_HOME/funky-moose-mix-analyzer` or `~/.local/share/funky-moose-mix-analyzer`

For tests or portable development, override this location:

```bash
MIX_ANALYZER_DATA_DIR=/tmp/mix-analyzer-data python3 app.py
```

## Build

macOS:

```bash
./build_mac.sh
```

Windows:

```bat
build_win.bat
```

## Quick Checks

```bash
python3 -m py_compile app.py
python3 -m unittest discover -s tests
node --check static/js/app.js
```

Useful local endpoints while developing:

- `GET /health` checks app data storage and ffmpeg availability.
- `GET /genres` returns the shared backend genre-profile list used by the UI.

## Notes

The repository currently contains historical build artifacts and generated media. The `.gitignore` prevents new generated outputs from being added again; a later cleanup can remove already tracked build artifacts from version control without changing the app source.
