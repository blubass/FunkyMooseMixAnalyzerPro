#!/bin/bash
set -e
pip install pyinstaller pywebview
pyinstaller --noconfirm --windowed --add-data "static:static" --add-data "templates:templates" --icon "assets/app_icon.icns" --name "Mix Analyzer" app.py
echo "Built: dist/Mix Analyzer.app"
