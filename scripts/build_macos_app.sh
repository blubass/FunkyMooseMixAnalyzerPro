#!/bin/bash
# Build macOS App
pip install pyinstaller
pyinstaller "Mix Analyzer.spec" --noconfirm
