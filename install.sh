#!/bin/bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$ROOT_DIR"

CEF_VERSION="156.0.15+gcf671c0+chromium-156.0.8078.80"

if [ "$(uname -s)" != "Darwin" ]; then
  echo "install.sh currently supports macOS only."
  exit 1
fi

ARCH="$(uname -m)"
if [ "$ARCH" = "arm64" ]; then
  CEF_PLATFORM="macosarm64"
  CMAKE_ARCH="arm64"
else
  CEF_PLATFORM="macosx64"
  CMAKE_ARCH="x86_64"
fi

for cmd in cmake curl tar; do
  if ! command -v "$cmd" >/dev/null 2>&1; then
    echo "Missing required command: $cmd"
    exit 1
  fi
done

CEF_ROOT="$ROOT_DIR/cef/$CEF_PLATFORM"
if [ ! -f "$CEF_ROOT/cmake/cef_variables.cmake" ]; then
  mkdir -p "$ROOT_DIR/cef"
  TMP_DIR="$ROOT_DIR/cef/.tmp.$$"
  mkdir -p "$TMP_DIR"
  ARCHIVE="cef_binary_${CEF_VERSION}_${CEF_PLATFORM}.tar.bz2"
  URL="https://cef-builds.spotifycdn.com/${ARCHIVE}"
  echo "Downloading CEF ${CEF_VERSION} (${CEF_PLATFORM})..."
  curl -fL --retry 3 "$URL" -o "$TMP_DIR/$ARCHIVE"
  echo "Extracting CEF..."
  tar -xjf "$TMP_DIR/$ARCHIVE" -C "$TMP_DIR"
  rm -rf "$CEF_ROOT"
  mv "$TMP_DIR/cef_binary_${CEF_VERSION}_${CEF_PLATFORM}" "$CEF_ROOT"
  rm -rf "$TMP_DIR"
else
  echo "CEF already present at $CEF_ROOT"
fi

echo "Configuring..."
cmake -B build -G Xcode \
  -DUSE_SANDBOX=OFF \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES="$CMAKE_ARCH" \
  -DCEF_ROOT="$CEF_ROOT"

echo "Building strome (Release)..."
cmake --build build --config Release --target strome

APP="$ROOT_DIR/build/src/Release/strome.app"
if [ -d "$APP" ]; then
  xattr -cr "$APP" >/dev/null 2>&1 || true
  codesign --force --sign - "$APP" >/dev/null 2>&1 || true
  echo
  echo "Build complete: $APP"
  echo "Run: open \"$APP\""
  echo "Or:  \"$APP/Contents/MacOS/strome\""
else
  echo "Build finished but app bundle not found at $APP"
  exit 1
fi
