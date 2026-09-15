# harbour-moji — Moji OCR

On-device OCR for Sailfish OS. Silica + the Mochi widget module, C++ core, CMake.

The app is called **Moji OCR** everywhere a user reads it — the `.desktop` name,
the README title, the RPM summary. The binary, the repo and the RPM are
`harbour-moji`. "Moji" (文字) is the Japanese for a written character, so it names
what the app does; the "OCR" suffix exists because nobody can be expected to know
that from the icon.

## The two constraints everything follows from

**1. Nothing here builds on the development machine.** The Sailfish SDK does not
run on an ARM Mac, so the RPM only ever comes out of GitHub Actions, on a tag.

- Logic goes in layers that plain Qt5 can test: `ocrresult`, `textlayout`,
  `fieldparser`, `imageprep`. Anything decided there is covered by `tests/`.
- `ocrengine` is the only file that talks to Tesseract, and it is a thin
  translation layer — no decisions in it. The models and the UI are covered by
  `tests/syntax-check.sh` and `scripts/check_qml.py` only.
- Before pushing, run the checks in a container (the command is in the README).
  A red CI on a tag has already spent the version number.

**2. Everything ships inside the RPM. No network, ever.** Not as a privacy
slogan — as a build rule:

- The app must not contain a single outbound request. There is no telemetry, no
  model download at first run, no "fetching language data".
- Native dependencies (Leptonica, Tesseract) are cross-compiled into `3rdparty/`
  inside the SDK container and cached on the build scripts' hash. There is no
  OpenCV: image preparation turned out to be scaling and a colour conversion,
  which `imageprep` does with `QImage` alone - and being free of OpenCV is what
  lets `tests/` reach it.
- Language data is downloaded **at build time**, verified against a pinned
  SHA256, and bundled. It is never committed — the repo carries no binaries — but
  the installed app has every byte it needs.
- Consequence: the RPM is large — 80MB of it is language data. That is the trade,
  and it is the point.
- The long tail ships as separate `harbour-moji-lang-*` packages, because all 126
  languages at once is 339MB. That is not a hole in the no-network rule: the app
  still never connects, the package manager does.

## Conventions

- Commits and tags in English, one line, concise. No emoji, no "by Claude".
- Prefer patch tags. A tag that failed is re-cut, not bumped.
- Silica best practices; `scripts/check_qml.py` encodes the ones already paid
  for. UI is built from `qml/Mochi/` — read `qml/Mochi/README.md` before adding a
  control, and do not mix Mochi controls with Silica's on one page.
- **A page's own actions are rows, not pulley entries.** The pulley carries what is
  global — settings, and that is now all it carries: About was beside it and moved
  into Settings, which already had a row for it. Two ways to one page, one of them
  behind a gesture, is a worse menu than one way in the obvious place. Mojo put the create action in the list as its last row
  rather than behind a button, and that is the gesture Mochi is for. Putting a
  page's verbs in the pulley is the Silica habit being replaced, and it is the easy
  mistake because a pulley is the quickest place to put something.
- **A photograph to measure against is worth more than an argument about
  recognition quality.** Everything above about segmentation modes, thresholding
  and rotation came out of five JPEGs and an hour of running the real pipeline over
  them in a container - Tesseract, plain Qt5 and the app's own `imageprep.cpp`, no
  SDK and no device. Drop photographs in `tests/fixtures/` (gitignored: the repo
  carries no binaries) and measure. Guessing at this is how PSM_SINGLE_BLOCK
  survived fourteen releases.
- **Two language lists, and they are not the same list.** The *interface* is
  English and French; every new `qsTr()` or `tr()` needs an entry in both
  catalogues, and `scripts/check_translations.py` names the ones you missed. The
  *recognition* languages are 30 of Tesseract's 126, listed in
  `scripts/download_models_for_build.sh`. Translating the UI into all 30 would be
  a great deal of work for nobody, so do not let the lists drift into each other.
- `rpm/*.yaml` is the packaging source of truth; the `.spec` is regenerated from
  it and committed so it can be read without the SDK. Edit both.
- Version lives in the tag. The build workflow writes it into the yaml, the spec
  and `CMakeLists.txt`; do not bump them by hand.
- CI/CD style follows https://github.com/nicosouv/lagoon.

## Things worth not relearning

- **Redaction is flattened into the pixels or it is not redaction.** The black
  bars are painted into a saved copy, not drawn as an overlay and not recorded in
  metadata: anything a viewer can switch off looks like redaction while being
  nothing of the sort. The boxes are drawn a few pixels proud of the word, because
  the recogniser's box is tight to the glyphs and a descender poking out from
  under a bar is enough to read a digit. Only IBANs, card numbers and passport
  codes are hidden by default - an email is found too, and deciding it is private
  would be deciding for the user.
- **The CSV table export was removed, and the reasoning is worth keeping.** A
  table is a block whose words pile into vertical bands, so the columns are the
  channels no word crosses, measured against the median word height - which is
  what made one threshold work at any camera distance. It worked. Nobody used it:
  a phone is not where a spreadsheet gets made, and the export wrote a file to
  Downloads that then had to be moved somewhere useful. `git log` has
  `tableextract.cpp` and its tests if the idea comes back; the lesson is that a
  feature being clever and correct is not the same as it being wanted.
- **OCR output is untrusted text.** It is whatever was in front of the camera. A
  `Label` with `textFormat: Text.RichText` showing recognised text will follow
  `<img>` tags that were photographed, which turns a picture into a network
  request. Render it as plain text. `scripts/check_qml.py` fails the build on
  this, the same way its ancestor in harbour-imtrix did for `formatted_body`.
- Tesseract wants raw pixels, not a file: `SetImage(data, w, h, bytesPerPixel,
  bytesPerLine)` takes a `QImage`'s bits directly. That is why Leptonica is built
  `--without-libpng --without-libjpeg --without-libtiff` — Qt already decodes
  every format we accept, and linking a second set of codecs doubles the size for
  nothing.
- Tesseract's `ResultIterator` is the whole reason this engine was chosen: it
  yields word, line, paragraph and block boxes with a confidence each. Tapping a
  word and growing the selection to its real block is only possible because that
  structure exists — a detector/recogniser pair returns strings and nothing else.
- **Poppler is the one dependency where the device is ahead of the host.** The
  build target (5.0.0.43) has `poppler-qt5` 24.08 and the phone (5.2.0.17) has
  25.12, against Ubuntu 24.04's 24.02 - all checked, none assumed. So
  `tests/syntax-check.sh` is genuinely representative for `pdfrender.cpp`, which is
  not true of anything else here. The soname is `libpoppler-qt5.so.1` on all three,
  so the `Requires` rpm generates from the link is satisfied: build against the
  older, run on the newer, which is the safe direction and the one CI already
  takes. It is also not
  cross-compiled: Poppler is on every device because the platform's own document
  viewer uses it, so it is a `BuildRequires` and a link line rather than a fourth
  thing in `3rdparty/`. The consequence to remember is the licence - Poppler is
  GPL-2.0-or-later, so the shipped binary is GPL even though this source stays
  MIT.
  Still written defensively: `Poppler::Document::load()` and `Document::page()`
  changed from raw pointers to `std::unique_ptr` around 22.x, and `pdfrender.cpp`
  has a two-overload `adopt()` so either spelling compiles. Both ends happen to be
  new enough today; the day Jolla pins an older one is not the day to find out.
- **Every off-device lane compiles against a newer Qt than the device has.**
  Ubuntu has 5.15, Sailfish has 5.6, so a call added in 5.8 passes
  `tests/syntax-check.sh` and fails inside the RPM build on a tag.
  `QDateTime::currentSecsSinceEpoch` cost a release that way;
  `scripts/check_qt56.py` holds the list, and the list grows rather than shrinks.
- **A QML import states the API version being asked for, not the one the device
  has.** Sailfish ships Qt 5.6, so `import QtQuick 2.5` resolves - but a file that
  says `2.0` and uses a 2.5 property does not merely lose that property: the whole
  file fails to load, the page comes up blank, and the reason is one line in the
  journal. `Image.autoTransform` cost a release this way;
  `scripts/check_qml.py` now knows the version each such property needs.
- **EXIF orientation is not applied by anything, by default.** A phone writes a
  portrait photo as the sensor's landscape frame plus a tag; `QImage(path)` ignores
  the tag, and so does QML's `Image` unless `autoTransform: true`. Both sides have
  to agree, or the preview is sideways relative to the boxes drawn on it.
  `ImagePrep::loadUpright()` is the only way a photo should be read here.
- **EXIF says how the phone was held, not how the text sits on the page.** A table
  or a spine caption is often turned against the paper, so recognition runs at 0,
  90 and 270 and keeps the best `readingScore()`. That score is mean confidence
  times the *square root* of the word count, and the square root is the whole
  point: a page read sideways does not find less, it finds far more, all of it
  fragments the recogniser openly doubts. A plain product picked 0 degrees on a
  real page where 90 was obviously right (321 words at 37.6% against 135 at
  74.7%). The numbers are in `tests/tst_correction.cpp`.
- **Quarter turns do nothing for a page held at seven degrees**, which is the
  usual case and costs a great deal. The tilt is measured from the baselines the
  winning pass already reported - `PageIterator::Baseline`, so it is free - and
  the page is read once more, straightened, keeping that result only if it scored
  better. `QImage::transformed` preserves `Format_Grayscale8` for a quarter turn
  and **not** for an arbitrary angle: it returns ARGB32 there, which would have
  Tesseract read every fourth byte. `ImagePrep::turnedBy` converts back, and a
  test pins it.
- Mapping a box out of an arbitrary rotation uses `QImage::trueMatrix`, never a
  hand-built transform: Qt translates the rotated result to keep it at the origin
  and that offset is not worth re-deriving.
- **The second binding loop was two items sizing each other across dimensions.**
  The frame holding the photo took its height from the canvas's width, while the
  canvas took its width from the frame's height. Same failure as below, and it
  survived because it was only reachable by tapping "rotate the view" - which
  nothing did until the view started turning itself. `check_qml.py` now follows
  size bindings between ids and refuses a cycle. Break one by deriving both ends
  from something outside the pair.
- **A binding loop does not look like a bug, it looks like a hang.** Qt prints
  "Binding loop detected" once and then keeps re-evaluating the layout, so on a
  device the page appears frozen and the log line scrolls past unread. The one
  that shipped sized an Item from a child Image's `paintedHeight` while that
  Image was anchored to fill the Item. Take proportions from `implicitWidth` /
  `implicitHeight`, which are what the loader decoded and depend on nothing in the
  layout; `scripts/check_qml.py` now fails the build on the other spelling.
  Worth knowing: a headless harness does **not** reliably reproduce it, because Qt
  reports re-entrancy rather than non-convergence and the evaluation order
  differs - which is why this is a static check and not a test.
- **Two Tesseract initialisation traps, both of which look like something else.**
  `Init()`'s datapath means different things in different versions - 3.x appended
  `tessdata/` to it, 4.x treats it as the directory that holds the files - and the
  failure either way is "Error opening data file", which names a path without
  saying which rule produced it. `OcrEngine` therefore tries both and keeps the
  one that works. Do not replace that with whichever single answer looks right;
  it has already been wrong here in both directions.
  And the engine must be named: `scripts/build_tesseract.sh` passes
  `--disable-legacy`, so `OEM_DEFAULT` can resolve to a recogniser that is not in
  the binary, which aborts the process instead of returning an error. Always
  `OEM_LSTM_ONLY`.
- A null `QString` binds as SQL NULL. `HistoryStore` has `text TEXT NOT NULL`, so
  recording a photo that turned out to have no text in it failed silently until a
  test caught it. Coerce, or make the column nullable - but decide, rather than
  finding out.
- **Tesseract's API default page-segmentation mode is `PSM_SINGLE_BLOCK`, not
  `PSM_AUTO`.** The command-line tool sets 3 before every run; a library caller who
  says nothing gets 6, and 6 means "this whole image is one uniform block of text".
  This engine never set one, so every photograph was read that way for fourteen
  releases. Always state the mode.
  Worth knowing about the shape of the damage: on a clean page held the right way
  up, 6 is nearly as good as 3 - 69 words at 89.3% against 66 at 91.9%. What it
  costs is everything else. Where 6 goes wrong it does not find less, it finds
  hundreds of things it does not believe, and `conf * sqrt(n)` then *prefers* that
  reading. The pipeline before and after the whole set of changes, same five
  photographs, same language data:

  | photograph | before | after |
  |---|---|---|
  | magazine page | 69 words at 89.3% | 66 at 91.9% |
  | dense spread | 486 words at 55.0%, 5.0s | 214 at 85.6%, 2.8s |
  | poster, at night | 644 words at 26.6%, 3.9s | 63 at 70.4%, 1.0s |
  | road sign, at night | 939 words at 20.5%, 13.1s | 7 at 67.3%, 0.8s |
  | sign behind a fence | 680 words at 20.9%, 7.3s | 11 at 50.0%, 1.2s |

  Note the times. Reading a sign went from thirteen seconds of garbage to under a
  second of text, because the garbage was what was expensive.
- **A sign is not a page, and page mode returns nothing at all on one.** Not "worse
  results" - zero words, at every angle, on a photograph of a street sign.
  `PSM_SPARSE_TEXT` reads it. But sparse mode reports no blocks and no paragraphs,
  and the tap-to-widen gesture and the table extractor are built on those, so page
  mode runs first and only a photograph that came back with nothing pays for the
  second search.
- **This camera writes EXIF orientation 1 on everything.** Five photographs off the
  device, all 8192x6144 - the sensor's own landscape frame - all tagged as needing
  no rotation, whichever way the phone was held. So `loadUpright` and
  `Image.autoTransform` are both working correctly and both doing nothing, and the
  angle search is not a refinement on top of EXIF: it is the only thing in the app
  that knows which way up a photograph is. Which is why the search must cover all
  four quarter turns - 180 was missing, and a sign photographed upside down could
  never be read. And why the result page turns the view to `ocr.orientation`: the
  recogniser is the only thing that found out.
- **Local thresholding is the largest free gain on a page and ruins a photograph
  taken at night.** In a dark frame every speck of sensor noise is darker than its
  neighbours, so Bradley-Roth marks all of it: measured, two document photographs
  came out 5.8% and 23.8% ink, three night ones 41%, 45% and 57%. Tesseract then
  called 189 specks words at 17% confidence - and `conf * sqrt(n)` rated that above
  the seven real words the untouched image gave, so the score picked the garbage.
  The fix is not a better score, it is not thresholding an image that is mostly
  noise; `ImagePrep::MaxInk` is where that line sits, and the numbers above are
  where it came from.
- **QLocale cannot resolve Tesseract's language codes.** `QLocale("fra")` is
  `QLocale::C`: Qt's table holds ISO 639-1 and Tesseract uses ISO 639-2/T. Anything
  built on that comparison silently does nothing - the picker listed "fra", "ces",
  "chi_sim" for fourteen releases, and the "default follows the phone's language"
  behaviour never once fired, so a French phone read French pages as English.
  `src/languagenames.h` is the table, and one test asserts the QLocale hole itself
  so the table can be deleted if a future Qt closes it.
- **Tesseract is not state of the art, and knowing where it is weak is the job.**
  It is trained on clean scans near 300 DPI; a hand-held photo of a glossy page at
  an angle is close to its worst case. The levers, in order of what they are
  actually worth here: say the right page segmentation mode (free, and it was
  wrong); local binarisation where it helps (free - `ImagePrep::binarised`, because
  Tesseract's internal Otsu picks *one* threshold for the whole page and a shadow
  gradient defeats that); and a different engine entirely, which is a project
  rather than a change.
- **`tessdata_best` is not one of those levers, on photographs.** This file used to
  claim it was "four times the size for a real accuracy gain". Measured through the
  pipeline as it stands, on the five photographs, `fra` best (4.0MB) against fast
  (1.1MB):

  | photograph | fast | best |
  |---|---|---|
  | magazine page | 66 words at 91.9% | 66 at 91.8%, same text |
  | dense spread | 214 at 85.6% | 214 at 86.5% |
  | poster, at night | 63 at 70.4% | 61 at 72.8% — clearly better text |
  | road sign | 7 at 67.3% | 7 at 62.8%, same text |
  | sign behind a fence | 11 at 50.0% | 13 at 31.3%, and it picked the wrong angle |

  One win, one regression, three ties, for 3.5x the bytes - and no time difference
  either way, which was also not expected. Its reputation comes from clean scans,
  which is what it was measured on. Do not spend the RPM on it without measuring
  the photographs you actually care about.
- `TessBaseAPI` is not thread-safe and initialisation is slow. One instance,
  owned by the engine, driven from a worker thread via `QtConcurrent`; never one
  per request.
- **`qreal` is `double` on aarch64 and `float` on armv7hl.** So `qMax(1.0, x)`
  where x is a qreal deduces two different types on 32-bit and refuses to compile
  there - while building perfectly on every lane that checks this code, all of
  which are 64-bit. Say `qMax<qreal>(...)`. `scripts/check_qt56.py` catches the
  other spelling.
- `%{_libdir}` is `/usr/lib` on armv7hl and `/usr/lib64` on aarch64, so anything
  installed there goes through `CMAKE_INSTALL_LIBDIR`, never a hardcoded `lib`.
  Getting it wrong builds cleanly on both and then fails packaging on aarch64
  alone, at `%files`, with an error that looks nothing like its cause.
- **The gallery picker needs `MediaIndexing`, not just `Pictures`.** `Pictures`
  whitelists `~/Pictures` on the filesystem; the picker lists images by asking the
  tracker index over D-Bus, which is a different thing and a different permission.
  Without it `ImagePickerPage` opens onto an empty page with no error at all — it
  is allowed to read the directory and not allowed to ask what is in it. Cost a
  release to find.

- The icon is extracted from a design mockup by `scripts/make_icons.py --extract`,
  not cropped by hand. The badge is semi-transparent over a photographed desk
  there, so a literal crop keeps a brown fringe that is invisible at full size and
  obvious on a dark ambience, and comes out washed out because the wood shows
  through. The script insets the mask past the fringe and puts the saturation
  back. `icons/harbour-moji.png` is the committed master at 407px; every shipped
  size is a downsample of it, so nothing is ever upscaled.
