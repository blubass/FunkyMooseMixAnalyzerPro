#!/bin/bash
set -euo pipefail

SOURCE_DIR="$(cd "$(dirname "$0")" && pwd)"
SOURCE_LINK="/private/tmp/fmma-plugin-src"
BUILD_DIR="${FMMA_PLUGIN_BUILD_DIR:-/private/tmp/fmma-plugin-build}"
ARTIFACT_DIR="$SOURCE_DIR/artifacts"
JUCE_PATH="${MIX_ANALYZER_JUCE_DIR:-${JUCE_DIR:-/Users/uwearthurfelchle/Developer/JUCE}}"
INSTALL_ARTIFACTS=0
INSTALL_SCOPE="user"
EXTRA_CMAKE_ARGS=()

usage() {
  cat <<'USAGE'
Usage: bash plugin/build_macos.sh [--install] [--user|--system] [cmake options]

Options:
  --install            Install built VST3, AU and standalone artefacts after build.
  --user               Install to current user locations (default).
  --system             Install to system locations; may require sudo.
  -h, --help           Show this help message.
USAGE
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --install)
      INSTALL_ARTIFACTS=1
      ;;
    --user)
      INSTALL_SCOPE="user"
      ;;
    --system)
      INSTALL_SCOPE="system"
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      EXTRA_CMAKE_ARGS+=("$1")
      ;;
  esac
  shift
done

if [[ ! -d "$JUCE_PATH" ]]; then
  echo "JUCE not found: $JUCE_PATH" >&2
  echo "Set MIX_ANALYZER_JUCE_DIR=/path/to/JUCE or JUCE_DIR=/path/to/JUCE." >&2
  exit 1
fi

ln -sfn "$SOURCE_DIR" "$SOURCE_LINK"
mkdir -p "$BUILD_DIR" "$ARTIFACT_DIR"

sign_bundle() {
  local bundle="$1"
  if command -v codesign >/dev/null 2>&1; then
    codesign --force --deep --sign - "$bundle"
  fi
}

cmake -S "$SOURCE_LINK" -B "$BUILD_DIR" -DMIX_ANALYZER_JUCE_DIR="$JUCE_PATH" "${EXTRA_CMAKE_ARGS[@]}"
cmake --build "$BUILD_DIR" --config Release --target FunkyMooseMixAnalyzer_Standalone FunkyMooseMixAnalyzer_AU FunkyMooseMixAnalyzer_VST3

ARTEFACT_ROOT="$BUILD_DIR/FunkyMooseMixAnalyzer_artefacts"

if [[ -d "$ARTEFACT_ROOT/VST3/Funky Moose Mix Analyzer.vst3" ]]; then
  ditto "$ARTEFACT_ROOT/VST3/Funky Moose Mix Analyzer.vst3" "$ARTIFACT_DIR/Funky Moose Mix Analyzer.vst3"
  sign_bundle "$ARTIFACT_DIR/Funky Moose Mix Analyzer.vst3"
fi

if [[ -d "$ARTEFACT_ROOT/AU/Funky Moose Mix Analyzer.component" ]]; then
  ditto "$ARTEFACT_ROOT/AU/Funky Moose Mix Analyzer.component" "$ARTIFACT_DIR/Funky Moose Mix Analyzer.component"
  sign_bundle "$ARTIFACT_DIR/Funky Moose Mix Analyzer.component"
fi

if [[ -d "$ARTEFACT_ROOT/Standalone/Funky Moose Mix Analyzer.app" ]]; then
  ditto "$ARTEFACT_ROOT/Standalone/Funky Moose Mix Analyzer.app" "$ARTIFACT_DIR/Funky Moose Mix Analyzer.app"
  sign_bundle "$ARTIFACT_DIR/Funky Moose Mix Analyzer.app"
fi

if [[ "$INSTALL_ARTIFACTS" -eq 1 ]]; then
  bash "$SOURCE_DIR/install_macos.sh" --$INSTALL_SCOPE
fi

echo "Built plugin artefacts in: $ARTIFACT_DIR"
