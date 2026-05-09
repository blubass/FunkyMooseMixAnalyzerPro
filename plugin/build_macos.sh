#!/bin/bash
set -euo pipefail

SOURCE_DIR="$(cd "$(dirname "$0")" && pwd)"
SOURCE_LINK="/private/tmp/fmma-plugin-src"
BUILD_DIR="${FMMA_PLUGIN_BUILD_DIR:-/private/tmp/fmma-plugin-build}"
ARTIFACT_DIR="$SOURCE_DIR/artifacts"
JUCE_PATH="${MIX_ANALYZER_JUCE_DIR:-${JUCE_DIR:-/Users/uwearthurfelchle/Developer/JUCE}}"

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

cmake -S "$SOURCE_LINK" -B "$BUILD_DIR" -DMIX_ANALYZER_JUCE_DIR="$JUCE_PATH" "$@"
cmake --build "$BUILD_DIR" --config Release

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

echo "Built plugin artefacts in: $ARTIFACT_DIR"
