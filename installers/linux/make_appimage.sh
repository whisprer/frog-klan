#!/usr/bin/env bash
set -euo pipefail

APP_NAME="$1"
VERSION="$2"
DIST_LINUX="$3"
OUT_DIR="$4"
ENTRY_BIN="$5"   # frogklan-linux

ARCH="x86_64"
WORK="$(mktemp -d)"
APPDIR="$WORK/${APP_NAME}.AppDir"

mkdir -p "$APPDIR/usr/bin" "$APPDIR/usr/share/${APP_NAME}"

# Copy payload beside the entry binary (and any libs/assets)
cp -a "${DIST_LINUX}/." "$APPDIR/usr/share/${APP_NAME}/"

# AppRun launches the entry binary
cat > "$APPDIR/AppRun" <<EOF
#!/usr/bin/env bash
HERE="\$(dirname "\$(readlink -f "\$0")")"
chmod +x "\$HERE/usr/share/${APP_NAME}/${ENTRY_BIN}" 2>/dev/null || true
exec "\$HERE/usr/share/${APP_NAME}/${ENTRY_BIN}" "\$@"
EOF
chmod +x "$APPDIR/AppRun"

# Desktop entry (minimal)
mkdir -p "$APPDIR/usr/share/applications"
cat > "$APPDIR/usr/share/applications/${APP_NAME}.desktop" <<EOF
[Desktop Entry]
Type=Application
Name=${APP_NAME}
Exec=${APP_NAME}
Icon=${APP_NAME}
Categories=Utility;
EOF

# Optional icon: dist/linux/frogklan.png
if [ -f "${DIST_LINUX}/${APP_NAME}.png" ]; then
  cp "${DIST_LINUX}/${APP_NAME}.png" "$APPDIR/${APP_NAME}.png"
fi

if ! command -v appimagetool >/dev/null 2>&1; then
  echo "ERROR: appimagetool not found in PATH."
  echo "Install AppImageKit's appimagetool and retry."
  exit 1
fi

mkdir -p "$OUT_DIR"
appimagetool "$APPDIR" "$OUT_DIR/${APP_NAME}-${VERSION}-${ARCH}.AppImage"

rm -rf "$WORK"
echo "OK: $OUT_DIR/${APP_NAME}-${VERSION}-${ARCH}.AppImage"
