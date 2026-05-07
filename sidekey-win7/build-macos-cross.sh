#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
BUILD="$ROOT/build/mingw"
APP_DIST="$ROOT/dist/app"

if ! command -v cmake >/dev/null 2>&1; then
  echo "Missing cmake. Install it with: brew install cmake"
  exit 1
fi

if command -v i686-w64-mingw32-g++ >/dev/null 2>&1; then
  ARCH="win32"
  CXX_COMPILER="i686-w64-mingw32-g++"
  RC_COMPILER="i686-w64-mingw32-windres"
  OBJDUMP="i686-w64-mingw32-objdump"
elif command -v x86_64-w64-mingw32-g++ >/dev/null 2>&1; then
  ARCH="x64"
  CXX_COMPILER="x86_64-w64-mingw32-g++"
  RC_COMPILER="x86_64-w64-mingw32-windres"
  OBJDUMP="x86_64-w64-mingw32-objdump"
else
  echo "Missing MinGW-w64. Install it with: brew install mingw-w64"
  exit 1
fi

rm -rf "$BUILD" "$APP_DIST"
mkdir -p "$APP_DIST"

cmake -S "$ROOT" -B "$BUILD" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_SYSTEM_NAME=Windows \
  -DCMAKE_CXX_COMPILER="$CXX_COMPILER" \
  -DCMAKE_RC_COMPILER="$RC_COMPILER"

cmake --build "$BUILD" --config Release --parallel

if [ -f "$BUILD/sidekey.exe" ]; then
  cp "$BUILD/sidekey.exe" "$APP_DIST/sidekey.exe"
elif [ -f "$BUILD/Release/sidekey.exe" ]; then
  cp "$BUILD/Release/sidekey.exe" "$APP_DIST/sidekey.exe"
else
  echo "Build finished, but sidekey.exe was not found under $BUILD"
  exit 1
fi

pushd "$ROOT" >/dev/null
"$RC_COMPILER" -O coff -i installer/installer.rc -o dist/installer.res.o
popd >/dev/null
"$CXX_COMPILER" \
  -std=c++17 \
  -DUNICODE \
  -D_UNICODE \
  -DWIN32_LEAN_AND_MEAN \
  -D_WIN32_WINNT=0x0601 \
  -mcrtdll=msvcrt \
  -municode \
  -mwindows \
  -static \
  -static-libgcc \
  -static-libstdc++ \
  -Wl,--major-subsystem-version,6,--minor-subsystem-version,1 \
  "$ROOT/installer/installer.cpp" \
  "$ROOT/dist/installer.res.o" \
  -o "$ROOT/dist/SideKey-Win7-Setup.exe" \
  -luser32 \
  -lshell32 \
  -ladvapi32 \
  -lole32 \
  -luuid

echo "Built $ARCH installer:"
echo "$ROOT/dist/SideKey-Win7-Setup.exe"

if command -v "$OBJDUMP" >/dev/null 2>&1; then
  IMPORTS="$("$OBJDUMP" -p "$ROOT/dist/SideKey-Win7-Setup.exe")"
  if [[ "$IMPORTS" == *api-ms-win-crt* ]]; then
    echo "Note: this Homebrew MinGW build imports Windows UCRT; old Windows 7 SP1 machines may need the VC++ Redistributable x86 or the Universal C Runtime update."
  fi
fi
