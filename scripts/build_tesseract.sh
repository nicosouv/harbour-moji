#!/bin/bash
# Cross-compiles Tesseract into 3rdparty/tesseract. Needs 3rdparty/leptonica,
# so run scripts/build_leptonica.sh first.
#
# Run inside the Sailfish SDK container, under sb2:
#
#   docker run --rm -v $(pwd):/home/mersdk/src:z \
#     coderus/sailfishos-platform-sdk:5.0.0.43 \
#     bash -c "cd /home/mersdk/src && sb2 -t SailfishOS-5.0.0.43-aarch64 \
#              bash scripts/build_tesseract.sh"
#
# 4.1.3 rather than 5.x, for two reasons. It is the line tessdata_fast 4.1.0 was
# published for, and its headers are C++11 - a 5.x header pulled into the app
# would drag the whole app up to C++17, which is a larger change to make for a
# device toolchain than the newer engine is worth.
#
# The legacy engine is off: it is the pre-neural-network recogniser, it needs its
# own much larger data files, and nothing here ever selects it. Training tools,
# the graphics viewer and libcurl are off for the same reason - none of them run
# on a phone.
set -euo pipefail

TESSERACT_VERSION="4.1.3"
TESSERACT_SHA256="83dc56b544be938983f528c777e4e1d906205b0f6dc0110afc223f2cc1cec6d3"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$SCRIPT_DIR/.."
PREFIX="$ROOT/3rdparty/tesseract"
LEPTONICA="$ROOT/3rdparty/leptonica"
WORK="$ROOT/.build-tesseract"

if [ ! -f "$LEPTONICA/lib/liblept.so" ]; then
    echo "3rdparty/leptonica is missing; run scripts/build_leptonica.sh first" >&2
    exit 1
fi

if [ -f "$PREFIX/lib/libtesseract.so" ]; then
    echo "tesseract already built in 3rdparty/tesseract"
    exit 0
fi

rm -rf "$WORK"
mkdir -p "$WORK" "$PREFIX"
cd "$WORK"

TARBALL="tesseract-${TESSERACT_VERSION}.tar.gz"
URL="https://github.com/tesseract-ocr/tesseract/archive/refs/tags/${TESSERACT_VERSION}.tar.gz"

echo "fetching $TARBALL"
curl -sfL "$URL" -o "$TARBALL"

actual="$(sha256sum "$TARBALL" | cut -d' ' -f1)"
if [ "$actual" != "$TESSERACT_SHA256" ]; then
    echo "checksum mismatch for $TARBALL" >&2
    echo "  expected: $TESSERACT_SHA256" >&2
    echo "  actual:   $actual" >&2
    exit 1
fi

tar xf "$TARBALL"
cd "tesseract-${TESSERACT_VERSION}"

# Tesseract finds Leptonica through pkg-config, so point it at the one just built
# rather than at anything the SDK might happen to have.
export PKG_CONFIG_PATH="$LEPTONICA/lib/pkgconfig:${PKG_CONFIG_PATH:-}"

./autogen.sh

./configure \
    --prefix="$PREFIX" \
    --disable-static \
    --enable-shared \
    --disable-legacy \
    --disable-openmp \
    --disable-graphics \
    --without-curl \
    --without-archive \
    LIBLEPT_HEADERSDIR="$LEPTONICA/include"

make -j"$(nproc)"
make install

rm -rf "$WORK"

echo
echo "tesseract $TESSERACT_VERSION installed into 3rdparty/tesseract"
ls -lh "$PREFIX/lib/"
