@echo off
setlocal

echo Preparing binaries...
if not exist bin mkdir bin

for /f "delims=" %%F in ('where ffmpeg 2^>nul') do (
  copy /Y "%%F" "bin\ffmpeg.exe" >nul
  goto ffmpeg_done
)
echo ffmpeg.exe not found in PATH.
exit /b 1
:ffmpeg_done

for /f "delims=" %%F in ('where ffprobe 2^>nul') do (
  copy /Y "%%F" "bin\ffprobe.exe" >nul
  goto ffprobe_done
)
echo ffprobe.exe not found in PATH.
exit /b 1
:ffprobe_done

python -m pip install -r requirements.txt pyinstaller
python -m PyInstaller ^
  --noconfirm ^
  --clean ^
  --windowed ^
  --add-data "static;static" ^
  --add-data "templates;templates" ^
  --add-data "bin;bin" ^
  --icon "assets\icon.ico" ^
  --name "Mix Analyzer" ^
  app.py

echo Built: dist\Mix Analyzer\Mix Analyzer.exe
pause
