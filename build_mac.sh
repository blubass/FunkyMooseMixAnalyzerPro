#!/bin/bash
set -euo pipefail

echo "Preparing binaries..."
mkdir -p bin
rm -f bin/ffmpeg bin/ffprobe
ffmpeg_path="$(command -v ffmpeg || true)"
ffprobe_path="$(command -v ffprobe || true)"
if [[ -z "$ffmpeg_path" || -z "$ffprobe_path" ]]; then
  echo "ffmpeg and ffprobe must be installed and available in PATH."
  exit 1
fi
cp "$ffmpeg_path" bin/ffmpeg
cp "$ffprobe_path" bin/ffprobe
chmod +x bin/ffmpeg bin/ffprobe

echo "Installing dependencies..."
python3 -m pip install -r requirements.txt pyinstaller

echo "Building App with PyInstaller..."
python3 -m PyInstaller \
  --noconfirm \
  --clean \
  --windowed \
  --add-data "static:static" \
  --add-data "templates:templates" \
  --add-data "bin:bin" \
  --icon "assets/app_icon.icns" \
  --name "Mix Analyzer" \
  app.py

echo "Built: dist/Mix Analyzer.app"
