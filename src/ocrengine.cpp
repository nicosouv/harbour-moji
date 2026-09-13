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

void OcrEngine::recognise(const QUrl &imageUrl, const QString &languages)
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

    m_watcher.setFuture(QtConcurrent::run(this, &OcrEngine::run, path, languages));
}

OcrEngine::Outcome OcrEngine::run(const QString &path, const QString &languages)
{
    Outcome outcome;

    qCDebug(lcMoji) << "reading" << path << "with" << languages;

    QImage source(path);
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

    // Raw pixels straight from the QImage. Leptonica never reads the file, which
    // is why it is built without any image codecs: Qt already decoded it.
    m_api->SetImage(prepared.image.constBits(),
                    prepared.image.width(),
                    prepared.image.height(),
                    1,  // Format_Grayscale8: one byte per pixel
                    static_cast<int>(prepared.image.bytesPerLine()));

    qCDebug(lcMoji) << "image set, recognising";

    if (m_api->Recognize(nullptr) != 0) {
        outcome.error = tr("Nothing could be read from that image.");
        return outcome;
    }

    qCDebug(lcMoji) << "recognised, walking the result";

    tesseract::ResultIterator *it = m_api->GetIterator();
    if (!it) {
        outcome.error = tr("Nothing could be read from that image.");
        return outcome;
    }

    outcome.result.setImageSize(source.size());

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
        // Back into the original photo's coordinates, so an overlay drawn from
        // these lands on the picture the user is looking at rather than on the
        // scaled copy they never see.
        entry.box = ImagePrep::toSourceRect(
            QRect(QPoint(left, top), QPoint(right - 1, bottom - 1)), prepared.scale);
        entry.confidence = it->Confidence(level);
        entry.line = qMax(0, line);
        entry.paragraph = qMax(0, paragraph);
        entry.block = qMax(0, block);

        outcome.result.append(entry);
    } while (it->Next(level));

    delete it;

    // Clearing releases the pixel buffer's hold on the prepared image, which is
    // about to go out of scope anyway - but Tesseract keeps the pointer, and a
    // later call would otherwise read freed memory.
    m_api->Clear();

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
