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

    QImage source(path);
    if (source.isNull()) {
        outcome.error = tr("That file is not an image this device can read.");
        return outcome;
    }

    const ImagePrep::Prepared prepared = ImagePrep::prepare(source);
    if (prepared.isNull()) {
        outcome.error = tr("The image could not be prepared for reading.");
        return outcome;
    }

    QMutexLocker locker(&m_apiMutex);

    if (!m_api) {
        m_api = new tesseract::TessBaseAPI();
    }

    // Init is tens of milliseconds per language, so it is done again only when
    // the language set actually changed.
    if (m_apiLanguages != languages) {
        if (m_api->Init(m_tessdataPath.toUtf8().constData(),
                        languages.toUtf8().constData()) != 0) {
            m_apiLanguages.clear();
            outcome.error = tr("The language data could not be loaded.");
            return outcome;
        }
        m_apiLanguages = languages;
    }

    // Raw pixels straight from the QImage. Leptonica never reads the file, which
    // is why it is built without any image codecs: Qt already decoded it.
    m_api->SetImage(prepared.image.constBits(),
                    prepared.image.width(),
                    prepared.image.height(),
                    1,  // Format_Grayscale8: one byte per pixel
                    static_cast<int>(prepared.image.bytesPerLine()));

    if (m_api->Recognize(nullptr) != 0) {
        outcome.error = tr("Nothing could be read from that image.");
        return outcome;
    }

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
