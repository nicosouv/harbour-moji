#!/bin/bash
# Cross-compiles Leptonica into 3rdparty/leptonica.
#
# Run inside the Sailfish SDK container, under sb2, which is what supplies the
# cross toolchain:
#
#   docker run --rm -v $(pwd):/home/mersdk/src:z \
#     coderus/sailfishos-platform-sdk:5.0.0.43 \
#     bash -c "cd /home/mersdk/src && sb2 -t SailfishOS-5.0.0.43-aarch64 \
#              bash scripts/build_leptonica.sh"
#
# CI caches the result on this file's hash, so editing it forces a rebuild - which
# is the point, and the reason the versions are pinned here rather than passed in.
#
# Every image codec is switched off. Leptonica normally links libpng, libjpeg,
# libtiff, giflib and libwebp so it can open files; Moji never asks it to open a
# file, because Qt has already decoded the photo and OcrEngine hands Tesseract the
# raw QImage bits. Linking a second set of codecs would add several megabytes to
# the RPM to duplicate something Qt does better.
set -euo pipefail

LEPTONICA_VERSION="1.82.0"
LEPTONICA_SHA256="155302ee914668c27b6fe3ca9ff2da63b245f6d62f3061c8f27563774b8ae2d6"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$SCRIPT_DIR/.."
PREFIX="$ROOT/3rdparty/leptonica"
WORK="$ROOT/.build-leptonica"

if [ -f "$PREFIX/lib/liblept.so" ]; then
    echo "leptonica already built in 3rdparty/leptonica"
    exit 0
fi

rm -rf "$WORK"
mkdir -p "$WORK" "$PREFIX"
cd "$WORK"

TARBALL="leptonica-${LEPTONICA_VERSION}.tar.gz"
URL="https://github.com/DanBloomberg/leptonica/releases/download/${LEPTONICA_VERSION}/${TARBALL}"

echo "fetching $TARBALL"
curl -sfL "$URL" -o "$TARBALL"

# Verified rather than trusted: this is a build input that ends up inside the
# shipped package, so it gets the same treatment as the language data.
actual="$(sha256sum "$TARBALL" | cut -d' ' -f1)"
if [ "$actual" != "$LEPTONICA_SHA256" ]; then
    echo "checksum mismatch for $TARBALL" >&2
    echo "  expected: $LEPTONICA_SHA256" >&2
    echo "  actual:   $actual" >&2
    exit 1
fi

tar xf "$TARBALL"
cd "leptonica-${LEPTONICA_VERSION}"

./configure \
    --prefix="$PREFIX" \
    --disable-static \
    --enable-shared \
    --without-libpng \
    --without-libjpeg \
    --without-libtiff \
    --without-giflib \
    --without-libwebp \
    --without-libopenjpeg \
    --disable-programs

make -j"$(nproc)"
make install

rm -rf "$WORK"

echo
echo "leptonica $LEPTONICA_VERSION installed into 3rdparty/leptonica"
ls -lh "$PREFIX/lib/"
