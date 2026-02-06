#!/usr/bin/env bash
set -euo pipefail

APP_NAME="$1"
VERSION="$2"
DIST_MAC="$3"
OUT_DIR="$4"
APP_BUNDLE="$5"     # frogklan-mac.app

WORK="$(mktemp -d)"
ROOT="$WORK/root"

mkdir -p "$ROOT/Applications"
cp -a "${DIST_MAC}/${APP_BUNDLE}" "$ROOT/Applications/${APP_BUNDLE}"

mkdir -p "$OUT_DIR"

# Unsigned pkg. For signing:
# pkgbuild --sign "Developer ID Installer: YOUR NAME (TEAMID)" ...
pkgbuild \
  --root "$ROOT" \
  --identifier "com.${APP_NAME}.pkg" \
  --version "$VERSION" \
  "$OUT_DIR/${APP_NAME}-${VERSION}.pkg"

rm -rf "$WORK"
echo "OK: $OUT_DIR/${APP_NAME}-${VERSION}.pkg"
