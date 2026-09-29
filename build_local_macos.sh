#!/usr/bin/env bash
# The flags match the "Build QPDF OSX" step in
# QuestPDF.Native.Qpdf.Build/.github/workflows/main.yml.
#
# Usage: ./build_local_macos.sh [destination-folder]
# The result is build/output/libqpdf.dylib. If a destination folder is given,
# the file is also copied there.

set -euo pipefail
cd "$(dirname "$0")"

export MACOSX_DEPLOYMENT_TARGET="12.0"
QPDF_CXX_FLAGS="-Os -ffunction-sections -fdata-sections -fvisibility=hidden -fvisibility-inlines-hidden"

rm -rf build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_STATIC_LIBS=OFF -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DUSE_IMPLICIT_CRYPTO=OFF -DREQUIRE_CRYPTO_NATIVE=ON -DCMAKE_CXX_FLAGS="$QPDF_CXX_FLAGS" -DCMAKE_SHARED_LINKER_FLAGS="-Wl,-dead_strip"
cmake --build build --parallel --target libqpdf

# Copy the real file (not the symlink) and strip local symbols, like CI does.
mkdir -p build/output
cp -L build/libqpdf/libqpdf.dylib build/output/libqpdf.dylib
strip -x build/output/libqpdf.dylib

if [ $# -gt 0 ]; then
  mkdir -p "$1"
  cp build/output/libqpdf.dylib "$1/libqpdf.dylib"
  echo "Copied to $1/libqpdf.dylib"
fi

ls -la build/output/libqpdf.dylib
