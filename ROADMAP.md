# Roadmap

Where Moji OCR is, and what is left. Kept honest: something is only under
**Done** if it has shipped in a tagged release and been seen working.

Current release: **v0.1.14**. Ten test suites, all green.

## Done

### Reading

- **Thirty languages, entirely offline.** No `Internet` permission at all, so the
  Sailjail sandbox refuses every outbound connection — the guarantee is the
  system's, not the developer's. All language data is inside the RPM.
- **The page is read whichever way up it is.** EXIF orientation is applied on
  load (nothing in Qt does this by default), then the page is recognised at
  several angles and the best reading kept — scored by mean confidence times the
  *square root* of the word count, because a page read sideways finds far more
  words, all of them fragments.
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
- **A PDF of the photo and its text, offline**: the photograph on the first page,
  the text on the pages after it. It carries the *amended* text — the version that
  laid the words invisibly over the photograph carried the raw reading instead, so
  a word corrected by hand went into the file uncorrected and nobody could see it
  had, because the text was invisible.
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

## Landed, not yet seen on a phone

Written, measured off-device against five photographs taken on the phone, and
green in CI — but nothing here has been held in a hand yet, so none of it is Done.
This is the list to try.

### Recognition

- **The half turn was missing.** The search tried 0, 90 and 270, so a photograph
  that arrived upside down could never be read at all. It is not an exotic case:
  this camera writes an EXIF orientation of 1 on every frame it takes — the
  sensor's own landscape frame, tagged "no rotation needed" however the phone was
  held — so which way up a picture arrives is whichever way up the sensor was, and
  one of the five was exactly inverted.
- **A page-segmentation mode is stated.** `TessBaseAPI`'s default is
  `PSM_SINGLE_BLOCK` — the command-line tool overrides it and a library caller who
  says nothing does not — so every photograph was read as one undivided slab of
  text.
- **A sign is not a page, and is asked a different question.** Page mode looks for
  columns and reading order, and on a photograph of a street sign it returns
  nothing at all — zero words, at all four angles. Sparse-text mode reads it. Page
  mode runs first because it is the one that reports structure, and only a
  photograph that has no structure pays for the second search.
- **Thresholding is refused when it would find noise.** Local thresholding is the
  largest free gain on a photograph of a page and the largest free way to ruin one
  taken at night: a dark frame thresholded against itself is a field of speckle,
  and the recogniser reads a couple of hundred specks as words. Measured, the two
  document photographs came out 5.8% and 23.8% ink and the three night ones 41%,
  45% and 57%, so the image says which case it is.
- **The default language never once worked.** It was found by comparing
  `QLocale(code).language()` against the phone's, and `QLocale("fra")` is
  `QLocale::C`, so nothing ever matched and every phone quietly defaulted to
  reading English.

Together, on the five photographs that prompted all of this:

| photograph | before | after |
|---|---|---|
| magazine page | 69 words at 89.3% | 66 at 91.9% |
| dense spread | 486 words at 55.0%, 5.0s | 214 at 85.6%, 2.8s |
| poster, at night | 644 words at 26.6%, 3.9s | 63 at 70.4%, 1.0s |
| road sign, at night | 939 words at 20.5%, 13.1s | 7 at 67.3%, 0.8s |
| sign behind a fence | 680 words at 20.9%, 7.3s | 11 at 50.0%, 1.2s |

The times are not a typo. Reading a sign went from thirteen seconds of garbage to
under a second of text, because the garbage was what was expensive: a thresholded
night frame gives Tesseract nine hundred specks to think about. The last row is
still poor — it is a sign behind a wire fence — and it is the argument for item 3
under **Next**.

### Interface

- **Languages are named, not coded.** The picker said "fra", "ces", "chi_sim",
  "ell", because it asked QLocale and QLocale cannot answer: its table holds ISO
  639-1 and Tesseract's codes are ISO 639-2/T. Now a table, all 126 of them, each
  as its own name plus its English one where the two differ — "Français (French)",
  "Suomi (Finnish)", "简体中文 (Chinese, Simplified)" — which needs no catalogue
  entry in either interface language.
- **Every button says what it does before it does it.** The six glyphs under the
  photo take two taps: the first names the verb in a bubble over the button it
  belongs to, the second runs it. A row of unlabelled squares is unreadable until
  you have learnt it, and the way to learn it used to be a press-and-hold — a
  gesture you have to already know about in order to discover anything. Nothing
  fires from a tap that was only a question, which matters most for the two verbs
  that write a file.
- **The photo is shown the way it was read.** Nothing upstream knows which way up
  a photograph is, so the preview came up sideways while the text came out fine.
  The recogniser finds the answer on the way past; the view uses it. The rotate
  button still wins afterwards.
- **A binding loop that had not been reached yet.** The frame holding the photo
  took its height from the canvas's width while the canvas took its width from the
  frame's height — the kind of bug that reads on a device as the page having
  stopped rather than as anything being wrong. It was only reachable by tapping
  "rotate the view", which is presumably why it survived; the view now turns itself
  on every photograph, so it was about to be on the ordinary path.

### Reading PDFs

- **Documents can be picked, and a PDF is rendered before it is read.** Poppler
  does the rasterising, and it needed no cross-compile: Sailfish ships
  `poppler-qt5` as part of the platform because the system document viewer uses
  it, and the device's copy (24.08) is actually *newer* than the one the
  off-device syntax check compiles against (24.02) — the reverse of the Qt
  situation, and worth not forgetting.
- **The resolution is the only decision, so it is the only thing tested.** A PDF
  page has no pixels, it has a size in points, and both ways of choosing wrong are
  quiet: too low and the recogniser sees text four pixels tall and reports
  nothing, which reads as a bad PDF; too high and an A4 page is 140MB of ARGB32
  and the process is killed, which reads as a crash. Aim at 300 DPI, the
  resolution Tesseract's models were trained near, and come down only far enough
  to fit the budget `ImagePrep` imposes anyway. A floor under that was written
  first and a test killed it: an A0 poster at 72 DPI is already over budget.
- **A page that already carries text says so.** A PDF that was exported rather
  than scanned has the text in it exactly, and recognising a picture of that text
  can only be worse. It says so and reads it anyway, because a page can carry a
  text layer over half of what is on it and the reading is still what was asked
  for.
- **`Documents` added to Sailjail.** The same trap as `MediaIndexing`, one
  directory over: without it the picker opens on nothing, with no error at all.
- **The binary is now GPLv2+.** Poppler is GPL-2.0-or-later, so linking it makes
  the distributed binary GPL. The source here stays MIT and stays reusable as MIT
  — MIT is GPL-compatible, which is what makes the combination distributable — and
  the source offer the GPL asks for is this repository. See README.

### The cover

- **There wasn't one.** `qml/cover/` was an empty directory and `ApplicationWindow`
  never set `cover:`, so the app showed Sailfish's default — the icon and the name
  — which on this platform reads as unfinished.
- **Three states, because the app has three.** Idle, reading, read. The one that
  earns the file is *reading*: recognition takes a second or three on a photograph
  and longer on a document, so somebody who minimised mid-read can see whether it
  is still going, and which page of a PDF it reached.
- **It will not put a private number on the home screen.** When the reading
  contained an IBAN, a card number or a passport code it says so instead of
  showing the text — the same `sensitiveCount` that decides whether the result
  page offers to black them out. A cover is visible to anyone who glances at the
  phone, which is a different audience from the person holding it.
- **Camera and share**, which is the platform's maximum of two. The camera action
  pushes rather than replacing the stack: tapping it over an open result has not
  asked to lose that result.

### Not measurable from here

- **The camera tags which way up it was.** Nothing set the capture orientation, so
  the camera wrote EXIF 1 on every frame. It is set now — and this is the one item
  that could not be checked, because the mapping from how the phone is held to the
  tag has a single anchor: a page shot in portrait needed a quarter turn clockwise.
  The other three orientations follow by symmetry and are guesses. Nothing depends
  on it: a tag that comes out backwards costs a wrong thumbnail in the gallery,
  because the recogniser searches all four angles regardless.

## Next

In the order I would do them.

1. **Language packs.** The architecture is settled and `tessdata-manifest.txt`
   pins all 126 languages by git blob hash — which GitHub's API reports without
   the file being downloaded, so the manifest was written without pulling 339MB
   and `git hash-object` recomputes it locally. What is missing is the build:
   noarch RPMs, one per script group, attached to the release. No cross-compile
   needed — they are data. **96 languages are unreachable until this lands.**
2. **Better recognition models are not the answer, and this is now measured.**
   `tessdata_best` against the `tessdata_fast` that ships, through the pipeline as
   it stands, on the same five photographs: one clear win, one regression, three
   ties, for 3.5x the bytes and no time difference either way. Its reputation
   comes from clean scans, which is what it was benchmarked on. The numbers are in
   CLAUDE.md. The ceiling here is Tesseract itself, not the model inside it, which
   is what item 4 is for.
3. **`osd.traineddata` as a pack.** It would give orientation *and* script in one
   pass instead of four, cutting recognition time by roughly three quarters. 10MB,
   and an unverified question about whether it survives the `--disable-legacy`
   the engine is built with — which is why it is a pack and not a default.
4. **A scene-text engine, beside Tesseract rather than instead of it.** Started:
   `src/scenetext.h` holds the decision record and the detector's output stage,
   which is the part with no neural network in it and therefore the part `tests/`
   can reach.

   The case for it is measured rather than assumed. Tesseract is a document
   recogniser; asked for a page, it returns *zero words* from a night photograph
   of a street sign, at all four angles. Sparse-text mode rescues that to seven
   words at 67%, which is the difference between useless and poor, and that is
   where tuning ends. A detector/recogniser pair is built for the other case: one
   model says where the text is, another reads each piece, and nothing has to look
   like a page.

   ncnn for the runtime — C++11, no dependencies, cross-compiles the way Leptonica
   and Tesseract already do in `3rdparty/`, and a few hundred kilobytes against
   ONNX Runtime's fifteen megabytes and its demand for a newer toolchain than the
   SDK has. PP-OCR mobile for the models: DBNet to detect at about 4.7MB, SVTR-LCNet
   to read at about 10MB, downloaded at build time against a pinned SHA256 exactly
   as the language data is, and still not one outbound request from the app.

   What is left: the ncnn cross-compile script, the model pinning, the thin call
   that produces the probability map, and the recognition half. It does not
   replace Tesseract and is not meant to — a detector/recogniser pair returns
   strings and boxes and no structure, and tapping a word to grow the selection to
   its paragraph needs paragraphs to exist.
5. **Comparing two photos of the same document**, to see what changed between two
   versions of a contract.

## Known limits

- **Four recognition passes, and a difficult photo can cost eight.** The page is
  tried all four ways up, and a photograph that reads as nothing in page mode is
  tried all four again in sparse-text mode. This turned out to be *faster* than the
  three passes it replaced, on every photograph measured, because the passes that
  were slow were the ones finding hundreds of specks — but that is a happy accident
  of these five and not a guarantee. A large photo still takes seconds, and it is
  paid exactly when the user is already waiting.
- **The deskew has not been proven on a hard case.** It is covered by tests and
  it keeps its result only when it scores better, so it cannot make things worse;
  whether it makes them much better on a badly-angled photo is still unmeasured.
- **Mochi has diverged from harbour-imtrix.** The copy here is ahead — `Banner`,
  the native Silica variants in ambience mode, three themes. Converging them is a
  copy, but imtrix has a large amount of unrelated work in flight, so it is not
  this repository's to do right now.

## How this stays honest

Nothing here builds on the development machine, so the only feedback before a tag
is what plain Qt5 can check. That is why the layers that decide anything —
`ocrresult`, `textlayout`, `fieldparser`, `imageprep`, `languagenames`,
`scenetext`, `pdfpage`, `historystore`, `pdfexport` — hold no Tesseract and no Qt Quick.

Four classes of bug reached a release before becoming a static check, and each
one is now caught in CI rather than on a phone:

| What shipped | What catches it now |
|---|---|
| A binding loop that looked like a hang | `check_qml.py` rejects a size bound to a child's painted size |
| `Image.autoTransform` under `import QtQuick 2.0` | `check_qml.py` knows each property's minimum QtQuick |
| `QDateTime::currentSecsSinceEpoch`, added in Qt 5.8 | `check_qt56.py` holds the list of APIs newer than 5.6 |
| A `tr()` filed under the wrong context | `check_translations.py` anchors C++ contexts to column zero |
| Two items sizing each other across the two dimensions | `check_qml.py` follows the size bindings between ids and refuses a cycle |
| A function declared and called with its body deleted | the CI `link` lane builds and links the whole app, which `-fsyntax-only` never did |

The list grows rather than shrinks.
