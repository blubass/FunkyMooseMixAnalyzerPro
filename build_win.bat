@echo off
pip install pyinstaller
pyinstaller --noconfirm --windowed --name "Mix Analyzer" app.py
echo Built: dist\Mix Analyzer\Mix Analyzer.exe
pause
