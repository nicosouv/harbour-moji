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

    // Whether the page carries text already.
    //
    // Worth asking, because a PDF that was exported rather than scanned has the
    // text in it exactly, and recognising a picture of it can only be worse. The
    // app still offers to read it - a page can carry a text layer covering half of
    // what is on it - but it says so first.
    Q_INVOKABLE QString embeddedText(const QUrl &fileUrl, int pageNumber);

    Q_INVOKABLE QString lastError() const { return m_lastError; }

private:
    QString m_lastError;
};

#endif // PDFRENDER_H
