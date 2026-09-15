#ifndef PDFRENDER_H
#define PDFRENDER_H

#include <QObject>
#include <QString>
#include <QUrl>

// Turning a page of a PDF into an image the rest of the app already knows how to
// read.
//
// The only file that talks to Poppler, and a thin translation layer by the same
// rule ocrengine.cpp follows: no decisions in here. What resolution to render at
// is decided by pdfpage.h, which has no Poppler in it and is covered by tests.
//
// The output is a JPEG in the cache directory and the flow carries on unchanged
// from there - the result page, the searchable PDF export, the redaction and the
// history all work on an image file and none of them needs to learn what a PDF
// is.
//
// Poppler is GPL-2.0-or-later and is already on every Sailfish device, because
// the platform's own document viewer uses it. Linking it makes the distributed
// binary GPL even though this source stays MIT; MIT is GPL-compatible, so that is
// a licence the combined work can be distributed under, and the source offer is
// the repository. See README.
class PdfRender : public QObject
{
    Q_OBJECT

public:
    explicit PdfRender(QObject *parent = nullptr);

    // How many pages, or 0 if the file will not open as a PDF.
    //
    // Called before anything is rendered, because a document with one page should
    // simply be read and a document with forty should ask which one.
    Q_INVOKABLE int pageCount(const QUrl &fileUrl);

    // Whether the file is a PDF at all, from its first bytes and not its name.
    Q_INVOKABLE bool isPdf(const QUrl &fileUrl) const;

    // Renders one page - numbered from 1, the way a reader numbers them - into a
    // JPEG under the cache directory, and returns a file:// URL to it.
    //
    // An empty URL means it could not be rendered; lastError says why.
    Q_INVOKABLE QUrl renderPage(const QUrl &fileUrl, int pageNumber);

    // A small rendering of one page, for choosing between them by sight.
    //
    // Separate from renderPage rather than a parameter on it, because the two
    // want opposite things: a page to be read is rendered at the resolution
    // Tesseract wants and costs a couple of hundred milliseconds, while a
    // thumbnail wants to be cheap and there may be forty of them on screen.
    //
    // `edge` is the longest side in pixels.
    Q_INVOKABLE QUrl thumbnail(const QUrl &fileUrl, int pageNumber, int edge);

    // Whether the page carries text already.
    //
    // Worth asking, because a PDF that was exported rather than scanned has the
    // text in it exactly, and recognising a picture of it can only be worse. The
    // app still offers to read it - a page can carry a text layer covering half of
    // what is on it - but it says so first.
    Q_INVOKABLE QString embeddedText(const QUrl &fileUrl, int pageNumber);

    Q_INVOKABLE QString lastError() const { return m_lastError; }

    // Throws away rendered pages, oldest first, until at most MaxCachedPages
    // remain. Called after every render and once at startup.
    //
    // These are full-page images of whatever the user opened - a payslip, an
    // attestation, a medical letter - written in the clear, and nothing else would
    // ever remove them. A cache that grows without bound is a nuisance; a cache of
    // other people's documents that grows without bound is a different thing.
    Q_INVOKABLE void pruneCache();

    // Enough that stepping back and forth through a document does not re-render,
    // few enough that a forgotten cache is small.
    static const int MaxCachedPages = 8;

    // Thumbnails are kept apart from full pages and counted separately, because
    // they are a different size of thing in both senses: a forty-page document
    // makes forty of them at once, which would evict every full page under a
    // shared cap, and each is a few kilobytes rather than a few hundred.
    static const int MaxCachedThumbnails = 96;

private:
    QString cacheDirectory() const;
    QString thumbnailDirectory() const;
    void pruneDirectory(const QString &path, int keep);

    QString m_lastError;
};

#endif // PDFRENDER_H
