#include "pdfrender.h"

#include <memory>

#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QStandardPaths>

#include <poppler-qt5.h>

#include "imageprep.h"
#include "logging.h"
#include "pdfpage.h"

namespace {

// Poppler changed the return type of load() and page() from a raw pointer to a
// std::unique_ptr somewhere in the 22.x series. Both spellings have to compile,
// because the host package the syntax check uses is far newer than the one on the
// device, and that difference is invisible until the RPM build on a tag - which is
// the one place in this project that a mistake costs a version number.
//
// Two overloads and the compiler picks. Neither leaks: the raw one is adopted, the
// smart one is moved through.
template <typename T>
std::unique_ptr<T> adopt(T *owned)
{
    return std::unique_ptr<T>(owned);
}

template <typename T>
std::unique_ptr<T> adopt(std::unique_ptr<T> owned)
{
    return owned;
}

// Opened and closed for each call rather than held.
//
// A Poppler::Document keeps the file open, and the picker hands back a path the
// user may well be about to move or delete. Opening a PDF is milliseconds; the
// rendering is what costs, and that is paid once per page.
std::unique_ptr<Poppler::Document> open(const QString &path, QString *error)
{
    if (!PdfPage::looksLikePdf(path)) {
        *error = QObject::tr("That file is not a PDF.");
        return nullptr;
    }

    std::unique_ptr<Poppler::Document> document = adopt(Poppler::Document::load(path));
    if (!document) {
        *error = QObject::tr("That PDF could not be opened.");
        return nullptr;
    }
    if (document->isLocked()) {
        // Tried with an empty password, which is what an "owner password" PDF
        // opens with; a real user password cannot be guessed and is not worth a
        // prompt in an OCR app.
        if (!document->unlock(QByteArray(), QByteArray())) {
            *error = QObject::tr("That PDF is password-protected.");
            return nullptr;
        }
    }

    // Poppler renders with no antialiasing unless asked, and text rendered hard
    // against the pixel grid is measurably worse to recognise - the recogniser
    // was trained on scans, which are never that clean.
    document->setRenderHint(Poppler::Document::Antialiasing, true);
    document->setRenderHint(Poppler::Document::TextAntialiasing, true);

    return document;
}

QString localPath(const QUrl &url)
{
    return url.isLocalFile() ? url.toLocalFile() : url.toString();
}

} // namespace

PdfRender::PdfRender(QObject *parent)
    : QObject(parent)
{
}

bool PdfRender::isPdf(const QUrl &fileUrl) const
{
    return PdfPage::looksLikePdf(localPath(fileUrl));
}

int PdfRender::pageCount(const QUrl &fileUrl)
{
    m_lastError.clear();
    const std::unique_ptr<Poppler::Document> document =
        open(localPath(fileUrl), &m_lastError);
    return document ? document->numPages() : 0;
}

QString PdfRender::embeddedText(const QUrl &fileUrl, int pageNumber)
{
    m_lastError.clear();
    const std::unique_ptr<Poppler::Document> document =
        open(localPath(fileUrl), &m_lastError);
    if (!document) {
        return QString();
    }

    const std::unique_ptr<Poppler::Page> page = adopt(document->page(pageNumber - 1));
    if (!page) {
        m_lastError = tr("That page is not in the document.");
        return QString();
    }

    return page->text(QRectF()).trimmed();
}

QUrl PdfRender::renderPage(const QUrl &fileUrl, int pageNumber)
{
    m_lastError.clear();

    const QString path = localPath(fileUrl);
    const std::unique_ptr<Poppler::Document> document = open(path, &m_lastError);
    if (!document) {
        return QUrl();
    }

    const std::unique_ptr<Poppler::Page> page = adopt(document->page(pageNumber - 1));
    if (!page) {
        m_lastError = tr("That page is not in the document.");
        return QUrl();
    }

    // The one number that matters, and the one thing here that is a decision -
    // so it is made in pdfpage.cpp, which a test can reach.
    const qreal dpi = PdfPage::resolutionFor(page->pageSizeF(), ImagePrep::MaxEdge);
    qCDebug(lcMoji) << "rendering page" << pageNumber << "of" << path
                    << "at" << dpi << "dpi, page size" << page->pageSizeF();

    const QImage rendered = page->renderToImage(dpi, dpi);
    if (rendered.isNull()) {
        m_lastError = tr("That page could not be rendered.");
        return QUrl();
    }

    // Named from the document's path and page, so picking the same page twice
    // does not fill the cache with copies. Hashed rather than derived from the
    // name: two documents called scan.pdf in different folders are two documents.
    const QString key = QString::fromLatin1(
        QCryptographicHash::hash(path.toUtf8(), QCryptographicHash::Sha1).toHex());
    const QString cache = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    QDir().mkpath(cache);
    const QString target = QStringLiteral("%1/%2-p%3.jpg").arg(cache, key)
                               .arg(pageNumber);

    // JPEG at a quality that costs nothing visible. A PNG of a rendered A4 page
    // is several times larger for a difference the recogniser cannot see, and
    // this file is a temporary that the whole rest of the flow will read back.
    if (!rendered.save(target, "JPEG", 92)) {
        m_lastError = tr("The rendered page could not be saved.");
        return QUrl();
    }

    qCDebug(lcMoji) << "rendered to" << target << rendered.size();
    return QUrl::fromLocalFile(target);
}
