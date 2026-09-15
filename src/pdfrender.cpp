#include "pdfrender.h"

#include <memory>

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
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
    // Anything left behind by a previous run goes now, rather than lingering until
    // the next PDF is opened - which might be never.
    pruneCache();
}

QString PdfRender::cacheDirectory() const
{
    return QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
           + QStringLiteral("/pages");
}

QString PdfRender::thumbnailDirectory() const
{
    return QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
           + QStringLiteral("/thumbs");
}

void PdfRender::pruneDirectory(const QString &path, int keep)
{
    QDir dir(path);
    if (!dir.exists()) {
        return;
    }

    // Oldest first, so the newest `keep` survive.
    const QFileInfoList files =
        dir.entryInfoList(QStringList { QStringLiteral("*.jpg") },
                          QDir::Files, QDir::Time | QDir::Reversed);

    const int excess = files.size() - keep;
    for (int i = 0; i < excess; ++i) {
        if (!QFile::remove(files.at(i).absoluteFilePath())) {
            qCWarning(lcMoji) << "could not drop a cached page";
        }
    }
}

void PdfRender::pruneCache()
{
    pruneDirectory(cacheDirectory(), MaxCachedPages);
    pruneDirectory(thumbnailDirectory(), MaxCachedThumbnails);

    // Anything left where v0.1.16 and v0.1.17 put rendered pages, before there
    // were subdirectories. Nothing writes there now, so nothing would ever have
    // removed them.
    pruneDirectory(QStandardPaths::writableLocation(QStandardPaths::CacheLocation), 0);
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

QUrl PdfRender::thumbnail(const QUrl &fileUrl, int pageNumber, int edge)
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

    // The same arithmetic the full render uses, against a much smaller budget -
    // so a thumbnail of an A4 page is about 20 dpi, which is unreadable and is
    // exactly right: it is there to be recognised as a shape, not read.
    const qreal dpi = PdfPage::resolutionFor(page->pageSizeF(), edge);
    const QImage rendered = page->renderToImage(dpi, dpi);
    if (rendered.isNull()) {
        m_lastError = tr("That page could not be rendered.");
        return QUrl();
    }

    const QString cache = thumbnailDirectory();
    QDir().mkpath(cache);

    const QString key = QString::fromLatin1(
        QCryptographicHash::hash(path.toUtf8(), QCryptographicHash::Sha1).toHex());
    const QString target = QStringLiteral("%1/%2-t%3.jpg").arg(cache, key)
                               .arg(pageNumber);

    if (!rendered.save(target, "JPEG", 80)) {
        m_lastError = tr("The rendered page could not be saved.");
        return QUrl();
    }

    return QUrl::fromLocalFile(target);
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
    const QString cache = cacheDirectory();
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

    // Kept small, every time. A document read page by page would otherwise leave
    // one image per page behind it.
    pruneCache();

    return QUrl::fromLocalFile(target);
}
