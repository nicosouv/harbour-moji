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

status=0
checked=0

while IFS= read -r file; do
    if g++ -fsyntax-only -std=c++14 -fPIC $CFLAGS -Isrc "$file"; then
        checked=$((checked + 1))
    else
        echo "FAILED: $file"
        status=1
    fi
done < <(find src -name '*.cpp' | sort)

if [ $status -eq 0 ]; then
    echo "syntax check passed on $checked files"
fi

exit $status
