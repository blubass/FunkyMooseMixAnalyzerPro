#!/bin/bash
set -euo pipefail

SOURCE_DIR="$(cd "$(dirname "$0")" && pwd)"
ARTIFACT_DIR="$SOURCE_DIR/artifacts"
SCOPE="user"
RESET_CUBASE_CACHE=0

usage() {
  cat <<'USAGE'
Usage: bash plugin/install_macos.sh [--user|--system] [--reset-cubase-cache]

Options:
  --user                Install to ~/Library/Audio/Plug-Ins (default).
  --system              Install to /Library/Audio/Plug-Ins. May require sudo.
  --reset-cubase-cache  Rename Cubase 15's VST3 scan cache so it rescans.
USAGE
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --user)
      SCOPE="user"
      ;;
    --system)
      SCOPE="system"
      ;;
    --reset-cubase-cache)
      RESET_CUBASE_CACHE=1
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "Unknown option: $1" >&2
      usage >&2
      exit 2
      ;;
  esac
  shift
done

VST3_SRC="$ARTIFACT_DIR/Funky Moose Mix Analyzer.vst3"
AU_SRC="$ARTIFACT_DIR/Funky Moose Mix Analyzer.component"
APP_SRC="$ARTIFACT_DIR/Funky Moose Mix Analyzer.app"

if [[ ! -d "$VST3_SRC" ]]; then
  echo "Missing VST3 artefact: $VST3_SRC" >&2
  echo "Run: bash plugin/build_macos.sh" >&2
  exit 1
fi

if [[ "$SCOPE" == "system" ]]; then
  VST3_DEST_ROOT="/Library/Audio/Plug-Ins/VST3"
  AU_DEST_ROOT="/Library/Audio/Plug-Ins/Components"
  APP_DEST_ROOT="/Applications"
else
  VST3_DEST_ROOT="$HOME/Library/Audio/Plug-Ins/VST3"
  AU_DEST_ROOT="$HOME/Library/Audio/Plug-Ins/Components"
  APP_DEST_ROOT="$HOME/Applications"
fi

VST3_DEST="$VST3_DEST_ROOT/Funky Moose Mix Analyzer.vst3"
AU_DEST="$AU_DEST_ROOT/Funky Moose Mix Analyzer.component"
APP_DEST="$APP_DEST_ROOT/Funky Moose Mix Analyzer.app"

sign_bundle() {
  local bundle="$1"
  if command -v codesign >/dev/null 2>&1; then
    codesign --force --deep --sign - "$bundle"
  fi
}

mkdir -p "$VST3_DEST_ROOT"
ditto "$VST3_SRC" "$VST3_DEST"
sign_bundle "$VST3_DEST"
echo "Installed VST3: $VST3_DEST"

if [[ -d "$AU_SRC" ]]; then
  mkdir -p "$AU_DEST_ROOT"
  ditto "$AU_SRC" "$AU_DEST"
  sign_bundle "$AU_DEST"
  echo "Installed AU: $AU_DEST"
fi

if [[ -d "$APP_SRC" ]]; then
  mkdir -p "$APP_DEST_ROOT"
  ditto "$APP_SRC" "$APP_DEST"
  sign_bundle "$APP_DEST"
  echo "Installed standalone app: $APP_DEST"
fi

if [[ "$RESET_CUBASE_CACHE" -eq 1 ]]; then
  CUBASE_CACHE="$HOME/Library/Preferences/Cubase 15/Cubase Pro VST3 Cache (arm64)"
  if [[ -d "$CUBASE_CACHE" ]]; then
    BACKUP_PATH="$CUBASE_CACHE.bak-$(date +%Y%m%d-%H%M%S)"
    mv "$CUBASE_CACHE" "$BACKUP_PATH"
    echo "Renamed Cubase VST3 cache: $BACKUP_PATH"
  else
    echo "Cubase VST3 cache not found; Cubase will create it on next launch."
  fi
fi
