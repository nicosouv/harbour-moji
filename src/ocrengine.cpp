#include "ocrengine.h"

#include <QFileInfo>
#include <QImage>
#include <QMutexLocker>
#include <QtConcurrent>

#include <tesseract/baseapi.h>
#include <tesseract/resultiterator.h>

#include "fieldparser.h"
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
                          bool autoRotate)
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

    m_watcher.setFuture(QtConcurrent::run(this, &OcrEngine::run, path, languages,
                                          autoRotate));
}

OcrEngine::Outcome OcrEngine::run(const QString &path, const QString &languages,
                                 bool autoRotate)
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

    const ImagePrep::Prepared prepared = ImagePrep::prepare(source);
    if (prepared.isNull()) {
        outcome.error = tr("The image could not be prepared for reading.");
        return outcome;
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

    for (int angle : angles) {
        const QImage grey = ImagePrep::rotated(prepared.image, angle);

        // Raw pixels straight from the QImage. Leptonica never reads the file,
        // which is why it is built without any image codecs: Qt already decoded it.
        m_api->SetImage(grey.constBits(),
                        grey.width(),
                        grey.height(),
                        1,  // Format_Grayscale8: one byte per pixel
                        static_cast<int>(grey.bytesPerLine()));

        if (m_api->Recognize(nullptr) != 0) {
            qCDebug(lcMoji) << "  angle" << angle << "could not be recognised";
            m_api->Clear();
            continue;
        }

        tesseract::ResultIterator *it = m_api->GetIterator();
        if (!it) {
            m_api->Clear();
            continue;
        }

        OcrResult attempt;
        attempt.setImageSize(source.size());
        attempt.setOrientation(angle);

        // Tesseract reports no index for a line, paragraph or block - only
        // whether the word starts a new one. Counting the transitions is how the
        // flat list in OcrResult gets its hierarchy, and it is also why the order
        // words arrive in has to be preserved exactly.
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

            // Two transforms back, in order: out of the rotation that was applied
            // for this pass, then out of the downscale. Skip either and the
            // overlay lands somewhere plausible but wrong.
            const QRect inRotated(QPoint(left, top), QPoint(right - 1, bottom - 1));
            const QRect inPrepared =
                ImagePrep::unrotateRect(inRotated, angle, grey.size());
            entry.box = ImagePrep::toSourceRect(inPrepared, prepared.scale);

            entry.confidence = it->Confidence(level);
            entry.line = qMax(0, line);
            entry.paragraph = qMax(0, paragraph);
            entry.block = qMax(0, block);

            attempt.append(entry);
        } while (it->Next(level));

        delete it;

        // Clearing releases Tesseract's hold on the pixel buffer, which belongs
        // to a QImage that is about to be replaced by the next angle's.
        m_api->Clear();

        qCDebug(lcMoji) << "  angle" << angle << ":" << attempt.count() << "words,"
                        << "confidence" << attempt.meanConfidence()
                        << "score" << attempt.readingScore();

        if (!haveAny || attempt.readingScore() > best.readingScore()) {
            best = attempt;
            bestAngle = angle;
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

QVariantList OcrEngine::fields() const
{
    QVariantList list;
    for (const FieldParser::Field &field : FieldParser::scan(m_result.text())) {
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
