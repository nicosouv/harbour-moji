#!/usr/bin/env python3
"""Flags Qt APIs newer than the Qt that Sailfish ships.

Sailfish OS 5.0 is on Qt 5.6. Every lane that compiles this code off-device uses
whatever Qt the distribution has - 5.15 on Ubuntu - so a call added in 5.8
compiles perfectly everywhere except the only place it matters, and the failure
arrives during an RPM build on a tag, after the version number has been spent.

This is the C++ half of the same trap scripts/check_qml.py catches for QML
imports. Neither check is clever: they are both a list of things that have
already cost a release.

Add to the table when a newer API is reached for, rather than removing the entry
that caught it.
"""

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "src"

# symbol -> (Qt version it arrived in, what to use instead on 5.6)
TOO_NEW = {
    "currentSecsSinceEpoch": ("5.8", "QDateTime::currentMSecsSinceEpoch() / 1000"),
    "qAsConst": ("5.7", "a const reference, or qAsConst's two-line equivalent"),
    "QRandomGenerator": ("5.10", "qrand(), seeded once"),
    "Qt::SkipEmptyParts": ("5.14", "QString::SkipEmptyParts"),
    "Qt::KeepEmptyParts": ("5.14", "QString::KeepEmptyParts"),
    "QStringView": ("5.10", "QStringRef or const QString &"),
    "qExchange": ("5.14", "a plain swap"),
    "QMetaType::fromType": ("5.15", "qMetaTypeId<T>()"),
    "isRightToLeft": ("5.11", "QLocale::textDirection()"),
    "QSet::intersects": ("5.6", None),
}


def sources():
    for pattern in ("*.cpp", "*.h"):
        yield from sorted(SRC.rglob(pattern))


def strip_comments(line):
    return re.sub(r"//.*$", "", line)


def main():
    failures = 0
    checked = 0

    for path in sources():
        checked += 1
        for number, raw in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
            line = strip_comments(raw)
            for symbol, (version, instead) in TOO_NEW.items():
                if instead is None:
                    continue
                if re.search(r"\b" + re.escape(symbol) + r"\b", line):
                    rel = path.relative_to(ROOT)
                    print(f"{rel}:{number}: {symbol} needs Qt {version}; "
                          f"Sailfish has 5.6")
                    print(f"    {line.strip()}")
                    print(f"    use {instead}")
                    failures += 1

    print(f"\nchecked {checked} C++ files, {failures} problem(s)")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
