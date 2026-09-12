#!/usr/bin/env python3
"""Keeps the .ts files honest against the QML sources.

lupdate is not run locally - the Sailfish SDK does not run on this project's
development machine, and the .ts files are hand-edited - so nothing otherwise
notices when the two drift. And the drift is silent: a string with no entry,
or an entry under a context no file uses any more, builds and ships perfectly
and simply comes out in English.

Renaming a page is the sharp edge. Qt keys translations by context, and for
QML the context is the file's base name, so renaming a page orphans every
message it had, in every locale at once.
"""

import re
import sys
from pathlib import Path
from xml.etree import ElementTree

ROOT = Path(__file__).resolve().parent.parent
QML_DIR = ROOT / "qml"
SRC_DIR = ROOT / "src"
TS_DIR = ROOT / "translations"

TEMPLATE = "harbour-moji.ts"
# English is the source language: it carries only the handful of strings that
# read differently in the UI than in the code, not the whole catalogue.
#
# Two catalogues, not five. Moji's *interface* is English and French; the
# languages it can *read* are a separate matter entirely - 126 of them ship with
# Tesseract - and confusing the two would mean translating the UI into every
# language the OCR supports.
OVERRIDE_ONLY = {"harbour-moji-en.ts"}

QSTR = re.compile(r'qsTr\(\s*"((?:[^"\\]|\\.)*)"')
BLOCK_COMMENT = re.compile(r"/\*.*?\*/", re.S)
LINE_COMMENT = re.compile(r"//[^\n]*")

# C++ side. Adjacent string literals are one message to lupdate, so both of
# these capture a run of them and join it back together the same way.
LITERALS = r'((?:"(?:[^"\\]|\\.)*"\s*)+)'
CPP_TR = re.compile(r"\btr\(\s*" + LITERALS, re.S)
CPP_TRANSLATE = re.compile(
    r'QCoreApplication::translate\(\s*"(\w+)"\s*,\s*' + LITERALS, re.S)
# "void MatrixClient::login(" - what tr() takes its context from
CPP_METHOD = re.compile(r"^[\w:<>,\s\*&]*?\b(\w+)::\w+\s*\(", re.M)
ONE_LITERAL = re.compile(r'"((?:[^"\\]|\\.)*)"')


def strip_comments(text):
    """A qsTr() shown in a doc comment is documentation, not a string."""
    return LINE_COMMENT.sub("", BLOCK_COMMENT.sub("", text))


def unescape_qml(source):
    """QML escape sequences, the way lupdate would resolve them.

    A "\\n" in a QML literal is a newline in the .ts source, not two
    characters, which is why .ts files carry literal line breaks inside
    <source>.
    """
    return (source.replace("\\n", "\n").replace("\\t", "\t")
                  .replace("\\r", "\r").replace('\\"', '"')
                  .replace("\\\\", "\\"))


def qml_strings():
    """(context, source) pairs the QML actually asks to translate."""
    found = set()
    for path in sorted(QML_DIR.rglob("*.qml")):
        context = path.stem
        text = strip_comments(path.read_text(encoding="utf-8"))
        for source in QSTR.findall(text):
            found.add((context, unescape_qml(source)))
    return found


def join_literals(run):
    """Adjacent C++ string literals, concatenated as the compiler would."""
    return unescape_qml("".join(ONE_LITERAL.findall(run)))


def cpp_strings():
    """(context, source) pairs the C++ asks to translate.

    Half of what the user reads comes from here - every login failure, and
    the name of any room that has no name of its own - so leaving it out
    would let those strings go untranslated without anything noticing.
    """
    found = set()
    for path in sorted(SRC_DIR.rglob("*.cpp")):
        text = strip_comments(path.read_text(encoding="utf-8"))

        # An explicit context needs no guessing
        for context, run in CPP_TRANSLATE.findall(text):
            found.add((context, join_literals(run)))

        # tr() takes the enclosing class as its context, so walk the file and
        # remember which "Class::method" definition we are inside of.
        boundaries = [(m.start(), m.group(1)) for m in CPP_METHOD.finditer(text)]
        for match in CPP_TR.finditer(text):
            # QCoreApplication::translate also ends in "tr(" - skip those,
            # they were taken above with their real context
            if text[:match.start()].rstrip().endswith("QCoreApplication::"):
                continue
            context = None
            for position, name in boundaries:
                if position < match.start():
                    context = name
                else:
                    break
            if context:
                found.add((context, join_literals(match.group(1))))
    return found


def ts_strings(path):
    root = ElementTree.parse(path).getroot()
    found = set()
    for context in root.findall("context"):
        name = context.find("name").text
        for message in context.findall("message"):
            source = message.find("source")
            if source is not None and source.text is not None:
                found.add((name, source.text))
    return found


def report(title, pairs, limit=12):
    print(f"\n{title} ({len(pairs)}):")
    for context, source in sorted(pairs)[:limit]:
        shown = source if len(source) <= 60 else source[:57] + "..."
        print(f"    {context}: {shown}")
    if len(pairs) > limit:
        print(f"    ... and {len(pairs) - limit} more")


def main():
    wanted = qml_strings() | cpp_strings()
    template = ts_strings(TS_DIR / TEMPLATE)
    problems = 0

    missing = wanted - template
    if missing:
        report(f"{TEMPLATE}: strings the QML uses with no entry", missing)
        problems += len(missing)

    orphaned = template - wanted
    if orphaned:
        report(f"{TEMPLATE}: entries no QML file asks for any more", orphaned)
        problems += len(orphaned)

    for path in sorted(TS_DIR.glob("*.ts")):
        if path.name == TEMPLATE or path.name in OVERRIDE_ONLY:
            continue
        locale = ts_strings(path)

        absent = template - locale
        if absent:
            report(f"{path.name}: missing next to {TEMPLATE}", absent)
            problems += len(absent)

        extra = locale - template
        if extra:
            report(f"{path.name}: not in {TEMPLATE} any more", extra)
            problems += len(extra)

    catalogues = len(list(TS_DIR.glob("*.ts")))
    print(f"\nchecked {len(wanted)} strings across {catalogues} catalogues, "
          f"{problems} problem(s)")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
