#include "ocrengine.h"

#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QLineF>
#include <QImage>
#include <QMutexLocker>
#include <QtConcurrent>

#include <tesseract/baseapi.h>
#include <tesseract/resultiterator.h>

#include "fieldparser.h"
#include "tableextract.h"
#include "pdfexport.h"
#include "imageprep.h"
#include "logging.h"

OcrEngine::OcrEngine(const QString &tessdataPath, QObject *parent)
    : QObject(parent)
    , m_tessdataPath(tessdataPath)
{
    // Tesseract's datapath means different things in different versions: 3.x
    // appended "tessdata/" to whatever it was given, 4.x uses it as the directory
    // holding the .traineddata files. Getting it wrong yields "Error opening data
    // file", which names a path but not which of the two rules produced it.
    //
    // So both are tried, in the order 4.x wants, and the one that works is kept.
    // Guessing was already wrong once here, in both directions.
    m_datapathCandidates << tessdataPath
                         << QFileInfo(tessdataPath).absolutePath();

    connect(&m_watcher, &QFutureWatcher<Outcome>::finished,
            this, &OcrEngine::handleFinished);
}

OcrEngine::~OcrEngine()
{
    // The future holds a pointer to this; letting it run past destruction is how
    // a crash-on-exit gets written.
    m_watcher.waitForFinished();

    QMutexLocker locker(&m_apiMutex);
    if (m_api) {
        m_api->End();
        delete m_api;
    }
}

void OcrEngine::recognise(const QUrl &imageUrl, const QString &languages,
                          bool autoRotate, bool enhance)
{
    recogniseRegion(imageUrl, languages, autoRotate, enhance, 0, 0, 0, 0);
}

void OcrEngine::recogniseRegion(const QUrl &imageUrl, const QString &languages,
                                bool autoRotate, bool enhance,
                                int x, int y, int width, int height)
{
    if (m_busy) {
        qCWarning(lcMoji) << "already recognising; ignoring" << imageUrl;
        return;
    }

    const QString path = imageUrl.isLocalFile() ? imageUrl.toLocalFile()
                                                : imageUrl.toString();
    if (!QFileInfo::exists(path)) {
        setLastError(tr("That image is not there any more."));
        emit failed(m_lastError);
        return;
    }

    setLastError(QString());
    setBusy(true);

    // An empty rectangle means the whole photo, which is what recognise() sends.
    const QRect region(x, y, width, height);
    m_watcher.setFuture(QtConcurrent::run(this, &OcrEngine::run, path, languages,
                                          autoRotate, enhance, region));
}

bool OcrEngine::recogniseInto(const QImage &grey, OcrResult *out,
                              QVector<QLineF> *baselines)
{
    // Raw pixels straight from the QImage. Leptonica never reads the file, which
    // is why it is built without any image codecs: Qt already decoded it.
    //
    // Boxes come out in this image's own coordinates. Mapping them anywhere else
    // is the caller's job, because only the caller knows what it did to the image
    // to get here.
    m_api->SetImage(grey.constBits(),
                    grey.width(),
                    grey.height(),
                    1,  // Format_Grayscale8: one byte per pixel
                    static_cast<int>(grey.bytesPerLine()));

    if (m_api->Recognize(nullptr) != 0) {
        m_api->Clear();
        return false;
    }

    tesseract::ResultIterator *it = m_api->GetIterator();
    if (!it) {
        m_api->Clear();
        return false;
    }

    // Tesseract reports no index for a line, paragraph or block - only whether
    // the word starts a new one. Counting the transitions is how the flat list in
    // OcrResult gets its hierarchy, and it is also why the order words arrive in
    // has to be preserved exactly.
    int line = -1;
    int paragraph = -1;
    int block = -1;

    const tesseract::PageIteratorLevel level = tesseract::RIL_WORD;
    do {
        if (it->IsAtBeginningOf(tesseract::RIL_BLOCK)) {
            ++block;
        }
        if (it->IsAtBeginningOf(tesseract::RIL_PARA)) {
            ++paragraph;
        }
        if (it->IsAtBeginningOf(tesseract::RIL_TEXTLINE)) {
            ++line;

            if (baselines) {
                int x1 = 0, y1 = 0, x2 = 0, y2 = 0;
                if (it->Baseline(tesseract::RIL_TEXTLINE, &x1, &y1, &x2, &y2)) {
                    baselines->append(QLineF(x1, y1, x2, y2));
                }
            }
        }

        char *word = it->GetUTF8Text(level);
        if (!word) {
            continue;
        }

        const QString text = QString::fromUtf8(word).trimmed();
        delete[] word;

        if (text.isEmpty()) {
            continue;
        }

        int left = 0, top = 0, right = 0, bottom = 0;
        if (!it->BoundingBox(level, &left, &top, &right, &bottom)) {
            continue;
        }

        OcrWord entry;
        entry.text = text;
        entry.box = QRect(QPoint(left, top), QPoint(right - 1, bottom - 1));
        entry.confidence = it->Confidence(level);
        entry.line = qMax(0, line);
        entry.paragraph = qMax(0, paragraph);
        entry.block = qMax(0, block);

        out->append(entry);
    } while (it->Next(level));

    delete it;

    // Clearing releases Tesseract's hold on the pixel buffer, which belongs to a
    // QImage the caller is about to drop.
    m_api->Clear();
    return true;
}

OcrEngine::Outcome OcrEngine::run(const QString &path, const QString &languages,
                                 bool autoRotate, bool enhance, QRect region)
{
    Outcome outcome;

    qCDebug(lcMoji) << "reading" << path << "with" << languages;

    // loadUpright, not QImage(path): the camera writes a portrait photo as a
    // landscape frame plus an EXIF tag, and Qt ignores that tag unless asked.
    QImage source = ImagePrep::loadUpright(path);
    if (source.isNull()) {
        outcome.error = tr("That file is not an image this device can read.");
        return outcome;
    }

    qCDebug(lcMoji) << "decoded" << source.size();

    // Remembered before any crop replaces source: the overlay is drawn on the
    // whole photo, so that is the size every box has to be expressed against.
    const QSize fullSize = source.size();

    // Cropping before anything else, so the downscale budget is spent on the part
    // that was asked for rather than on the whole page. Everything downstream then
    // works in the crop's coordinates, and the offset is added back at the end.
    QPoint regionOffset;
    if (!region.isEmpty()) {
        const QRect clamped = region.intersected(source.rect());
        if (clamped.isEmpty()) {
            outcome.error = tr("That area is outside the photo.");
            return outcome;
        }
        source = source.copy(clamped);
        regionOffset = clamped.topLeft();
        qCDebug(lcMoji) << "restricted to" << clamped;
    }

    ImagePrep::Prepared prepared = ImagePrep::prepare(source);
    if (prepared.isNull()) {
        outcome.error = tr("The image could not be prepared for reading.");
        return outcome;
    }

    if (enhance) {
        // Thresholded against the local average. Tesseract would otherwise apply
        // one threshold to the whole page, which a photograph's lighting gradient
        // defeats - see ImagePrep::binarised.
        prepared.image = ImagePrep::binarised(prepared.image);
    }

    qCDebug(lcMoji) << "prepared" << prepared.image.size()
                    << "scale" << prepared.scale
                    << "format" << prepared.image.format();

    QMutexLocker locker(&m_apiMutex);

    if (!m_api) {
        m_api = new tesseract::TessBaseAPI();
    }

    // Init is tens of milliseconds per language, so it is done again only when
    // the language set actually changed.
    if (m_apiLanguages != languages) {
        // Checked before Init rather than after, so a missing language pack says
        // which file is missing instead of "could not be loaded".
        const QString first = languages.section(QLatin1Char('+'), 0, 0);
        const QString probe = m_tessdataPath + QLatin1Char('/') + first
                              + QStringLiteral(".traineddata");
        if (!QFileInfo::exists(probe)) {
            qCWarning(lcMoji) << "no language data at" << probe;
            outcome.error = tr("The language data could not be loaded.");
            return outcome;
        }

        // OEM_LSTM_ONLY, explicitly, and not the default.
        //
        // scripts/build_tesseract.sh passes --disable-legacy, so the old
        // pre-neural recogniser is not merely unused - it is not in the binary.
        // OEM_DEFAULT lets Tesseract decide, and if it decides on legacy it calls
        // into code that was compiled out, which aborts the process rather than
        // returning an error. Saying which engine we want keeps the build flag
        // and the call in agreement.
        int status = -1;
        for (const QString &candidate : m_datapathCandidates) {
            qCDebug(lcMoji) << "initialising tesseract with" << languages
                            << "datapath" << candidate;
            status = m_api->Init(candidate.toUtf8().constData(),
                                 languages.toUtf8().constData(),
                                 tesseract::OEM_LSTM_ONLY);
            if (status == 0) {
                break;
            }
            qCDebug(lcMoji) << "  that datapath did not work, status" << status;
        }

        if (status != 0) {
            m_apiLanguages.clear();
            qCWarning(lcMoji) << "tesseract Init failed for every datapath";
            outcome.error = tr("The language data could not be loaded.");
            return outcome;
        }
        m_apiLanguages = languages;
        qCDebug(lcMoji) << "tesseract ready";
    }

    // Recognise the page each way up and keep the best reading.
    //
    // EXIF says how the phone was held; it says nothing about how the text sits
    // on the page, and a table or a spine caption is often turned ninety degrees
    // against the paper. Tesseract's own OSD would answer this in one pass, but it
    // needs a 10MB model and it is not clear it survives the --disable-legacy this
    // is built with - so the answer is taken from the recogniser itself, which is
    // the thing that actually knows whether it could read what it was shown.
    //
    // The extra passes are only paid for when the first one comes out badly. A
    // page the right way up is usually obvious immediately.
    QVector<int> angles;
    angles << 0;
    if (autoRotate) {
        angles << 90 << 270;
    }

    OcrResult best;
    int bestAngle = 0;
    bool haveAny = false;
    QVector<QLineF> bestBaselines;

    for (int angle : angles) {
        const QImage grey = ImagePrep::rotated(prepared.image, angle);

        OcrResult attempt;
        QVector<QLineF> baselines;
        if (!recogniseInto(grey, &attempt, &baselines)) {
            qCDebug(lcMoji) << "  angle" << angle << "could not be recognised";
            continue;
        }

        attempt.setImageSize(source.size());
        attempt.setOrientation(angle);

        // Two transforms back, in order: out of the rotation applied for this
        // pass, then out of the downscale. Skip either and the overlay lands
        // somewhere plausible but wrong.
        for (int i = 0; i < attempt.count(); ++i) {
            const QRect inPrepared =
                ImagePrep::unrotateRect(attempt.words().at(i).box, angle, grey.size());
            attempt.setWordBox(i, ImagePrep::toSourceRect(inPrepared, prepared.scale));
        }

        qCDebug(lcMoji) << "  angle" << angle << ":" << attempt.count() << "words,"
                        << "confidence" << attempt.meanConfidence()
                        << "score" << attempt.readingScore();

        if (!haveAny || attempt.readingScore() > best.readingScore()) {
            best = attempt;
            bestAngle = angle;
            bestBaselines = baselines;
            haveAny = true;
        }

        // Good enough on the first try: a page read the right way up is not
        // marginal, and two more passes on a large photo is seconds of the user
        // waiting for a result that will not change.
        if (angle == 0 && attempt.count() >= 8 && attempt.meanConfidence() >= 75.0f) {
            break;
        }
    }

    if (!haveAny) {
        outcome.error = tr("Nothing could be read from that image.");
        return outcome;
    }

    qCDebug(lcMoji) << "best reading at" << bestAngle << "degrees";

    // Quarter turns put the page the right way up; they do nothing for a page
    // photographed at seven degrees, which is the usual case and costs a great
    // deal - Tesseract's line finder tolerates a little skew and gets steadily
    // worse through it. The tilt is measured from the baselines the winning pass
    // already reported, so this is one more recognition, not a search.
    const qreal skew = ImagePrep::skewAngle(bestBaselines);
    qCDebug(lcMoji) << "page tilt measured at" << skew << "degrees";

    if (autoRotate && qAbs(skew) >= ImagePrep::MinSkew
        && qAbs(skew) <= ImagePrep::MaxSkew) {

        const QImage squared = ImagePrep::rotated(prepared.image, bestAngle);
        const ImagePrep::Turned turned = ImagePrep::turnedBy(squared, -skew);

        OcrResult straightened;
        if (recogniseInto(turned.image, &straightened, nullptr)) {
            straightened.setImageSize(source.size());
            straightened.setOrientation(bestAngle);

            // Three transforms back, in order: out of the straightening, out of
            // the quarter turn, out of the downscale.
            for (int i = 0; i < straightened.count(); ++i) {
                const QRect inSquared = ImagePrep::untransformRect(
                    straightened.words().at(i).box, turned.transform);
                const QRect inPrepared =
                    ImagePrep::unrotateRect(inSquared, bestAngle, squared.size());
                straightened.setWordBox(i,
                    ImagePrep::toSourceRect(inPrepared, prepared.scale));
            }

            qCDebug(lcMoji) << "straightened:" << straightened.count() << "words,"
                            << "confidence" << straightened.meanConfidence()
                            << "score" << straightened.readingScore();

            // Kept only if it actually read better. Straightening by an angle the
            // measurement got wrong makes things worse, and the recogniser is the
            // only honest judge of that.
            if (straightened.readingScore() > best.readingScore()) {
                qCDebug(lcMoji) << "straightening helped; keeping it";
                best = straightened;
            }
        }
    }

    // Back into the whole photo's coordinates, if only part of it was read. Done
    // once at the end rather than threaded through every transform above.
    if (!regionOffset.isNull()) {
        for (int i = 0; i < best.count(); ++i) {
            best.setWordBox(i, best.words().at(i).box.translated(regionOffset));
        }
        best.setImageSize(fullSize);
    }

    outcome.result = best;
    return outcome;
}

void OcrEngine::handleFinished()
{
    const Outcome outcome = m_watcher.result();
    setBusy(false);

    if (!outcome.error.isEmpty()) {
        m_result.clear();
        emit resultChanged();
        setLastError(outcome.error);
        emit failed(outcome.error);
        return;
    }

    m_result = outcome.result;

    // A new reading replaces any amendment: the text it was made against is gone.
    m_edited = false;
    m_editedText.clear();
    emit editedTextChanged();

    qCDebug(lcMoji) << "read" << m_result.count() << "words, mean confidence"
                    << m_result.meanConfidence();

    emit resultChanged();
    emit finished();
}

QVariantMap OcrEngine::selectAt(int x, int y, int scope, int tolerance) const
{
    QVariantMap map;

    const TextLayout::Selection selection = TextLayout::selectAt(
        m_result, QPoint(x, y), static_cast<TextLayout::Scope>(scope), tolerance);

    map.insert(QStringLiteral("valid"), selection.isValid());
    if (!selection.isValid()) {
        return map;
    }

    map.insert(QStringLiteral("text"), selection.text);
    map.insert(QStringLiteral("words"), selection.words.size());
    map.insert(QStringLiteral("x"), selection.box.x());
    map.insert(QStringLiteral("y"), selection.box.y());
    map.insert(QStringLiteral("width"), selection.box.width());
    map.insert(QStringLiteral("height"), selection.box.height());
    return map;
}

QString OcrEngine::editedText() const
{
    return m_edited ? m_editedText : m_result.text();
}

void OcrEngine::setEditedText(const QString &text)
{
    if (text == editedText()) {
        return;
    }

    // Matching the recognised text again counts as not edited, so the flag does
    // not stay on after someone undoes their change.
    m_edited = (text != m_result.text());
    m_editedText = text;
    emit editedTextChanged();
}

QVariantList OcrEngine::fields() const
{
    QVariantList list;
    // Scanned from what the user has, not from what was recognised: fixing a
    // digit in the text box has to make the IBAN's checksum agree, the same way
    // correcting the word does.
    for (const FieldParser::Field &field : FieldParser::scan(editedText())) {
        QVariantMap map;
        map.insert(QStringLiteral("kind"), field.kind);
        // Untranslated: the UI names the kinds, because a C++ layer that calls
        // tr() decides the wording for every caller of it.
        map.insert(QStringLiteral("kindName"), FieldParser::kindName(field.kind));
        map.insert(QStringLiteral("raw"), field.raw);
        map.insert(QStringLiteral("value"), field.normalised);
        map.insert(QStringLiteral("checksumValid"), field.checksumValid);
        // Kinds that carry no checksum must not be shown as "verified": there was
        // nothing to verify, and a green tick would be a claim.
        map.insert(QStringLiteral("checkable"),
                   field.kind == FieldParser::Iban
                       || field.kind == FieldParser::CreditCard
                       || field.kind == FieldParser::Isbn
                       || field.kind == FieldParser::MrzLine);
        list.append(map);
    }
    return list;
}

QVariantList OcrEngine::lines() const
{
    QVariantList list;
    for (int line : m_result.lineNumbers()) {
        const QRect box = m_result.lineBox(line);
        QVariantMap map;
        map.insert(QStringLiteral("x"), box.x());
        map.insert(QStringLiteral("y"), box.y());
        map.insert(QStringLiteral("width"), box.width());
        map.insert(QStringLiteral("height"), box.height());
        map.insert(QStringLiteral("confidence"), m_result.lineConfidence(line));
        list.append(map);
    }
    return list;
}

QVariantList OcrEngine::uncertainWords() const
{
    QVariantList list;
    for (int index : m_result.uncertainWords()) {
        const OcrWord &word = m_result.words().at(index);
        QVariantMap map;
        map.insert(QStringLiteral("index"), index);
        map.insert(QStringLiteral("text"), word.text);
        map.insert(QStringLiteral("confidence"), word.confidence);
        map.insert(QStringLiteral("x"), word.box.x());
        map.insert(QStringLiteral("y"), word.box.y());
        map.insert(QStringLiteral("width"), word.box.width());
        map.insert(QStringLiteral("height"), word.box.height());
        list.append(map);
    }
    return list;
}

int OcrEngine::uncertainCount() const
{
    return m_result.uncertainWords().size();
}

QVariantList OcrEngine::blocks() const
{
    QVariantList list;
    for (int block : m_result.blockNumbers()) {
        const QRect box = m_result.blockBox(block);
        QVariantMap map;
        map.insert(QStringLiteral("block"), block);
        map.insert(QStringLiteral("words"), m_result.wordsInBlock(block).size());
        map.insert(QStringLiteral("x"), box.x());
        map.insert(QStringLiteral("y"), box.y());
        map.insert(QStringLiteral("width"), box.width());
        map.insert(QStringLiteral("height"), box.height());
        list.append(map);
    }
    return list;
}

QString OcrEngine::textOfBlock(int block) const
{
    return block < 0 ? m_result.text() : m_result.blockText(block);
}

QString OcrEngine::textExcluding(const QVariantList &blocks) const
{
    QVector<int> excluded;
    for (const QVariant &value : blocks) {
        excluded.append(value.toInt());
    }
    return excluded.isEmpty() ? m_result.text()
                              : m_result.textExcludingBlocks(excluded);
}

bool OcrEngine::exportPdf(const QUrl &imageUrl, const QString &path) const
{
    const QString source = imageUrl.isLocalFile() ? imageUrl.toLocalFile()
                                                  : imageUrl.toString();

    // Loaded upright, the same way recognition loaded it, or the page would be
    // written sideways with the text sitting correctly over nothing.
    const QImage photo = ImagePrep::loadUpright(source);
    if (photo.isNull()) {
        qCWarning(lcMoji) << "cannot read" << source << "to export it";
        return false;
    }

    return PdfExport::write(path, photo, m_result);
}

QVariantList OcrEngine::tables() const
{
    QVariantList list;
    for (int block : m_result.blockNumbers()) {
        const int columns = TableExtract::columnCount(m_result, block);
        // Two columns is the least that can be a table; one is a paragraph, and
        // offering a CSV export for a paragraph is worse than offering none.
        if (columns < 2) {
            continue;
        }

        const QRect box = m_result.blockBox(block);
        QVariantMap map;
        map.insert(QStringLiteral("block"), block);
        map.insert(QStringLiteral("columns"), columns);
        map.insert(QStringLiteral("rows"), TableExtract::cells(m_result, block).size());
        map.insert(QStringLiteral("x"), box.x());
        map.insert(QStringLiteral("y"), box.y());
        map.insert(QStringLiteral("width"), box.width());
        map.insert(QStringLiteral("height"), box.height());
        list.append(map);
    }
    return list;
}

QString OcrEngine::csvOfBlock(int block) const
{
    return TableExtract::toCsv(m_result, block);
}

bool OcrEngine::exportCsv(const QString &path, int block) const
{
    const QString csv = csvOfBlock(block);
    if (csv.isEmpty()) {
        return false;
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qCWarning(lcMoji) << "cannot write" << path << file.errorString();
        return false;
    }

    QTextStream out(&file);
    // Explicit UTF-8: a table off a French receipt is full of accented words, and
    // the local 8-bit codec would mangle them differently on every device.
    out.setCodec("UTF-8");
    out << csv << QLatin1Char('\n');

    return file.error() == QFile::NoError;
}

void OcrEngine::correctWord(int index, const QString &text)
{
    if (m_busy) {
        // The worker owns the result while it runs; editing it underneath would
        // be overwritten when the pass finishes, if it did not corrupt it first.
        return;
    }

    m_result.setWordText(index, text);
    qCDebug(lcMoji) << "corrected word" << index << "to" << text;

    // A per-word correction is authoritative, so it also discards a whole-block
    // amendment rather than leaving two versions of the truth.
    m_edited = false;
    m_editedText.clear();
    emit editedTextChanged();

    // One signal drives the text, the selection, the confidence and the field
    // scan, because all of them are computed from the result rather than cached.
    emit resultChanged();
}

int OcrEngine::growScope(int scope) const
{
    return TextLayout::grow(static_cast<TextLayout::Scope>(scope));
}

void OcrEngine::clear()
{
    m_result.clear();
    emit resultChanged();
}

void OcrEngine::setBusy(bool busy)
{
    if (busy == m_busy) {
        return;
    }
    m_busy = busy;
    emit busyChanged();
}

void OcrEngine::setLastError(const QString &error)
{
    if (error == m_lastError) {
        return;
    }
    m_lastError = error;
    emit lastErrorChanged();
}
