# Moji OCR

Reads text from photos, entirely on the device. For Sailfish OS.

*Moji* (文字) is the Japanese for a written character.

## Offline is enforced, not promised

The app has no `Internet` permission. Sailjail then makes the sandbox refuse
every outbound connection, so this is a property of the installed package rather
than a claim in a README — and it stays true whether or not the code deserves the
trust.

There is no account, no telemetry, and no model to fetch at first run: the
language data is inside the RPM, which is most of why the RPM is 90MB.

## Reading

30 languages are built in:

| | |
|---|---|
| Western Europe | English, French, German, Spanish, Italian, Portuguese, Dutch, Catalan |
| Nordics | Swedish, Danish, Norwegian, Finnish |
| Central & Eastern Europe | Polish, Czech, Hungarian, Romanian, Russian, Ukrainian, Greek, Turkish |
| Elsewhere | Arabic, Hebrew, Chinese (simplified and traditional), Japanese, Korean, Hindi, Thai, Vietnamese, Indonesian |

Several at once is normal, not an edge case — a French invoice with English words
in it is the common document — so Settings offers a language plus an optional
"also read English".

Tesseract ships 126 languages in total. All of them together are 339MB, which is
not an app, so the rest arrive as separate `harbour-moji-lang-*` packages. That
puts no network request inside Moji: the package manager fetches a pack, the way
every distribution has shipped dictionaries for thirty years. Installed packs
appear in Settings by themselves, because the list is read from the `tessdata`
directory rather than from a list in the code.

## What it does with the text

- **A searchable PDF**, written on the device. The photo with the recognised text
  laid invisibly over it: it looks exactly like the photograph and any reader can
  search, select and copy it. No server involved, which is the part nobody else
  manages.
- **Raw text**, to copy or export.
- **Tap a word in the photo** and the selection grows to the structure the
  recogniser actually found — the line, then the paragraph, then the block —
  rather than to a rectangle you have to drag.
- **Fields checked against their own checksums.** An IBAN is verified mod-97, a
  card number by Luhn, an ISBN by its check digit, a passport's machine-readable
  zone by all four of its. This matters more for text that came out of a camera
  than for text that was typed: OCR confuses 8 with B and 0 with O, which is
  exactly what a checksum exists to catch. A field that does not check out is
  reported as such, because "look at this line again" is more useful than a
  confident wrong answer.

What is done and what is next: [ROADMAP.md](ROADMAP.md).

## Installation

Download the RPM for your architecture from
[GitHub Releases](https://github.com/nicosouv/harbour-moji/releases), then:

```bash
pkcon install-local harbour-moji-*.rpm
```

## Interface

Built from `qml/Mochi/`, a small QML module that takes the webOS Mojo widget
vocabulary — grouped panels instead of section headers, hairlines with a barely
perceptible curve, the webOS switch — and fits it to Silica. See
[`qml/Mochi/README.md`](qml/Mochi/README.md).

Three themes, in Settings: **Ambience** (the default, following the user's
Sailfish ambience), **Mochi light** and **Mochi dark**.

The interface itself is in English and French. That is a different list from the
recognition languages above, and deliberately so: translating the UI into all 30
would be a great deal of work for no one's benefit.

## Building

### Architecture

- **C++ native**, **CMake**
- **Tesseract 4.1.3** (LSTM only) with **Leptonica 1.82**, cross-compiled and
  bundled into the RPM. Leptonica is built without any image codec: Qt has already
  decoded the photo, and `OcrEngine` hands Tesseract the raw `QImage` bits.
- Built for `aarch64` and `armv7hl`

Tesseract was chosen over a detector/recogniser model pair for one reason: its
`ResultIterator` yields word, line, paragraph and block boxes with a confidence
each. Tap-to-extract needs that structure to snap to; a pair of ONNX models
returns strings and nothing else.

Packaging lives in `rpm/harbour-moji.yaml`, which is the source of truth;
`rpm/harbour-moji.spec` is regenerated from it at build time and committed so it
can be read without the SDK.

### Releases

Cut by pushing a tag, which is also the single source of the version number — the
workflow rewrites `rpm/*.yaml`, `rpm/*.spec` and `CMakeLists.txt` from it:

```bash
git tag v0.1.1 && git push origin v0.1.1
```

### Running the checks locally

No Sailfish SDK needed, and no Qt on the host either:

```bash
docker run --rm -v "$PWD:/src:ro" -w /work ubuntu:24.04 bash -c '
  apt-get update -qq && apt-get install -y -qq --no-install-recommends \
    qtbase5-dev qtdeclarative5-dev libqt5sql5-sqlite cmake ninja-build \
    g++ pkg-config python3
  cp -r /src/. /work && cd /work
  python3 scripts/check_qml.py && python3 scripts/check_translations.py
  python3 scripts/check_qt56.py
  bash tests/syntax-check.sh
  cmake -S tests -B build-tests -G Ninja && cmake --build build-tests
  QT_QPA_PLATFORM=offscreen ctest --test-dir build-tests --output-on-failure'
```

### Native dependencies

`scripts/build_leptonica.sh` and `scripts/build_tesseract.sh` cross-compile them
inside the SDK container, under `sb2`. CI caches the result per architecture on
the hash of those two scripts, so editing one forces a rebuild and nothing else
does. Both tarballs are checksum-pinned, the same as the language data.

### Language data

`scripts/download_models_for_build.sh` fetches the 30 base languages from a
pinned tag of `tessdata_fast`, verifying each against a recorded SHA256. They are
never committed. Adding a language means adding it to `LANGUAGES` and pasting the
checksum the script prints — never guessing one, because an unverified download is
worse than no check at all: it looks like one.

### Tests

The layers that decide anything — `ocrresult`, `textlayout`, `fieldparser`,
`imageprep` — hold no Tesseract and no Qt Quick, so `tests/` reaches them with
plain Qt5 on any machine. That is deliberate: the RPM is only ever built in CI, on a tag, so this
is the only feedback available before a version number is spent.
