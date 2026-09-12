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
- `TessBaseAPI` is not thread-safe and initialisation is slow. One instance,
  owned by the engine, driven from a worker thread via `QtConcurrent`; never one
  per request.
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
