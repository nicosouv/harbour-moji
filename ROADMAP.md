# Roadmap

Where Moji OCR is, and what is left. Kept honest: something is only under
**Done** if it has shipped in a tagged release and been seen working.

Current release: **v0.1.14**. Eight test suites, 128 assertions, all green.

## Done

### Reading

- **Thirty languages, entirely offline.** No `Internet` permission at all, so the
  Sailjail sandbox refuses every outbound connection — the guarantee is the
  system's, not the developer's. All language data is inside the RPM.
- **The page is read whichever way up it is.** EXIF orientation is applied on
  load (nothing in Qt does this by default), then the page is recognised at 0, 90
  and 270 and the best reading kept — scored by mean confidence times the *square
  root* of the word count, because a page read sideways finds far more words, all
  of them fragments.
- **Fine deskew.** Quarter turns do nothing for a page held at seven degrees,
  which is the usual case. The tilt is measured from the baselines the winning
  pass already reported, the page is read once more straightened, and that result
  is kept only if it scored better.
- **Camera and gallery**, both.
- **The language can be changed on the result page**, not only in Settings —
  which is where you are standing when you discover it was wrong. The default
  follows the phone's own language.

### Taking the text out

- **Tap a word, tap again to widen.** The selection grows to the structure the
  recogniser actually found: word, line, paragraph, block. A fingertip may miss by
  a tolerance; containment still wins over proximity.
- **Blocks can be picked or dropped.** A photographed leaflet brings the next
  column with it; Tesseract had already separated them, so keeping one — or
  excluding one — needs no cropping and no second reading.
- **Read only an area**, for when the recogniser merged two columns into one
  block and picking will not do.
- **Doubtful words are marked and can be retyped.** Below 70% confidence a word
  is tinted; tapping it opens a field. The correction flows everywhere the
  original went — including the checksums, so fixing a digit the camera misread
  turns an IBAN from "does not match" to "verified".
- **The output is selectable**, so part of it can be copied rather than all.

### What it does that other OCR apps do not

- **Fields checked against their own checksums**: IBAN by mod-97, card numbers by
  Luhn, ISBN by its check digit, a passport's machine-readable zone by all four of
  its. A field that does not check out is reported as such, because "read this
  line again" beats a confident wrong answer — and OCR confuses 8 with B, which is
  exactly what a checksum is for.
- **Searchable PDF, offline**: the photo with an invisible text layer positioned
  over the words, so the page is searchable in any reader.
- **Tables to CSV**: a table is a block whose words pile into vertical bands, and
  the column boundaries are the channels no word crosses. Works on an unruled
  receipt. Refusing a paragraph matters as much as finding a table.
- **Private numbers painted out**: account and card numbers blacked into a saved
  copy — flattened into the pixels, because an overlay a viewer can switch off
  would look like redaction while being nothing of the sort.
- **A confidence map**: recognised lines are drawn on the photo, tinted where the
  recogniser was unsure, so you can see which parts to read twice.

### Around it

- **History** of what has been read, kept as text rather than as a second copy of
  every photo. Reopening shows the stored text; re-reading is asked for, not
  assumed.
- **Interface in English and French**, deliberately a different list from the
  thirty recognition languages.
- **Three themes**: Ambience (the default, following the user's Sailfish
  ambience), Mochi light and Mochi dark. In ambience mode the controls are real
  Silica ones, not lookalikes.

## Next

In the order I would do them.

1. **Language packs.** The architecture is settled and `tessdata-manifest.txt`
   pins all 126 languages by git blob hash — which GitHub's API reports without
   the file being downloaded, so the manifest was written without pulling 339MB
   and `git hash-object` recomputes it locally. What is missing is the build:
   noarch RPMs, one per script group, attached to the release. No cross-compile
   needed — they are data. **96 languages are unreachable until this lands.**
2. **`osd.traineddata` as a pack.** It would give orientation *and* script in one
   pass instead of three, cutting recognition time by roughly two thirds. 10MB,
   and an unverified question about whether it survives the `--disable-legacy`
   the engine is built with — which is why it is a pack and not a default.
3. **Comparing two photos of the same document**, to see what changed between two
   versions of a contract.

## Known limits

- **Three recognition passes cost time.** A large photo takes several seconds
  because the page is tried three ways up. The first pass short-circuits when it
  comes out clearly good, so this is only paid on a difficult page — but it is
  paid exactly when the user is already waiting.
- **The deskew has not been proven on a hard case.** It is covered by tests and
  it keeps its result only when it scores better, so it cannot make things worse;
  whether it makes them much better on a badly-angled photo is still unmeasured.
- **Two tables on one page**: the CSV export takes the first. Rare enough to wait
  until someone meets it.
- **Mochi has diverged from harbour-imtrix.** The copy here is ahead — `Banner`,
  the native Silica variants in ambience mode, three themes. Converging them is a
  copy, but imtrix has a large amount of unrelated work in flight, so it is not
  this repository's to do right now.

## How this stays honest

Nothing here builds on the development machine, so the only feedback before a tag
is what plain Qt5 can check. That is why the layers that decide anything —
`ocrresult`, `textlayout`, `fieldparser`, `imageprep`, `tableextract`,
`historystore`, `pdfexport` — hold no Tesseract and no Qt Quick.

Four classes of bug reached a release before becoming a static check, and each
one is now caught in CI rather than on a phone:

| What shipped | What catches it now |
|---|---|
| A binding loop that looked like a hang | `check_qml.py` rejects a size bound to a child's painted size |
| `Image.autoTransform` under `import QtQuick 2.0` | `check_qml.py` knows each property's minimum QtQuick |
| `QDateTime::currentSecsSinceEpoch`, added in Qt 5.8 | `check_qt56.py` holds the list of APIs newer than 5.6 |
| A `tr()` filed under the wrong context | `check_translations.py` anchors C++ contexts to column zero |

The list grows rather than shrinks.
