#!/usr/bin/env bash
# Compiles every C++ file in src/ far enough to catch syntax and signature
# errors, without the Sailfish SDK.
#
# The unit tests deliberately cover only the pure layers - the result structure,
# the layout queries, the field checksums - so nothing there ever compiles main,
# the settings or the engine. This does. It is a gate, not a build: no linking, no
# moc, and the objects are thrown away.
#
# It matters more here than in most projects: the RPM is only ever built in CI, on
# a tag, so without this a typo in settings.cpp would first be seen by a release
# that has already burned its version number.
set -u

CFLAGS=$(pkg-config --cflags Qt5Core Qt5Gui Qt5Qml Qt5Quick Qt5Concurrent Qt5Sql)

# ocrengine.cpp includes Tesseract's headers. The host package is used purely to
# have something to include - the version differs from the cross-compiled one, so
# this proves the call signatures are plausible, not that the ABI matches.
# Files that cannot be compiled without a library the host may not have. Skipped
# rather than failed, and the skip has to be real: the first version of this said
# "will not be checked" and then compiled the file anyway, so a machine without
# Poppler failed the lane with a missing header while claiming to have skipped it.
SKIP=""

if pkg-config --exists tesseract; then
    CFLAGS="$CFLAGS $(pkg-config --cflags tesseract lept)"
else
    echo "warning: no host tesseract; ocrengine.cpp will not be checked" >&2
    SKIP="$SKIP src/ocrengine.cpp"
fi

# pdfrender.cpp includes Poppler's headers, for the same reason and with one
# difference worth knowing: Sailfish ships poppler-qt5 as part of the platform and
# the device's copy is *newer* than the host's, which is the reverse of the Qt
# situation and makes this check unusually representative.
if pkg-config --exists poppler-qt5; then
    CFLAGS="$CFLAGS $(pkg-config --cflags poppler-qt5)"
else
    echo "warning: no host poppler-qt5; pdfrender.cpp will not be checked" >&2
    SKIP="$SKIP src/pdfrender.cpp"
fi

status=0
checked=0
skipped=0

while IFS= read -r file; do
    case " $SKIP " in
        *" $file "*)
            skipped=$((skipped + 1))
            continue
            ;;
    esac

    if g++ -fsyntax-only -std=c++14 -fPIC $CFLAGS -Isrc "$file"; then
        checked=$((checked + 1))
    else
        echo "FAILED: $file"
        status=1
    fi
done < <(find src -name '*.cpp' | sort)

if [ $status -eq 0 ]; then
    if [ $skipped -gt 0 ]; then
        echo "syntax check passed on $checked files ($skipped skipped, no host library)"
    else
        echo "syntax check passed on $checked files"
    fi
fi

exit $status
