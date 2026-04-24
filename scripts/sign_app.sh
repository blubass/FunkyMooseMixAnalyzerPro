#!/bin/bash
set -euo pipefail
APP_PATH="${1:-dist/Mix Analyzer.app}"
IDENTITY="${2:-}"
ICON="${3:-assets/icon.icns}"

if [ ! -d "$APP_PATH" ]; then
  echo "App bundle not found: $APP_PATH" >&2
  exit 1
fi

if [ -f "$ICON" ]; then
  /usr/bin/sips -s format icns "$ICON" --out "$APP_PATH/Contents/Resources/AppIcon.icns" >/dev/null 2>&1 || true
fi

if [ -z "$IDENTITY" ]; then
  echo "No identity provided. Using ad-hoc signature."
  /usr/bin/codesign --force --deep --sign - "$APP_PATH"
else
  echo "Signing with identity: $IDENTITY"
  /usr/bin/codesign --force --deep --options runtime --sign "$IDENTITY" "$APP_PATH"
fi

/usr/bin/codesign --verify --deep --strict --verbose=2 "$APP_PATH" || true
/usr/sbin/spctl -a -vv "$APP_PATH" || true
echo "Done."
