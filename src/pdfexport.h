#ifndef PDFEXPORT_H
#define PDFEXPORT_H

#include <QImage>
#include <QPointF>
#include <QSizeF>
#include <QString>

// Writing the photograph and the text it gave up, as one PDF.
//
// The photograph first, on its own page, then the text on the pages after it.
// Two things a reader can use: the picture, for anything the recogniser missed or
// got wrong, and the text, selectable and searchable because it is real text and
// not a picture of some.
//
// This used to be a "searchable PDF" - the photograph with the recognised words
// laid over it in a transparent pen, positioned box by box. That is the clever
// answer and it was the wrong one twice over. The text it carried was the raw
// reading, so a word corrected by hand went into the file uncorrected; and being
// invisible, nobody could check what it said. Text on its own page is plainer,
// carries the amendments, and can be read by a person rather than only found by a
// search.
namespace PdfExport {

// A4 at 72 points per inch, which is what QPdfWriter's default resolution means.
const qreal PageWidth = 595.0;
const qreal PageHeight = 842.0;

// The margin around the text pages. The photograph gets the whole page.
const qreal Margin = 40.0;

// How many points one image pixel becomes, fitting the image on the page without
// cropping or distorting it.
//
// Pure, and tested, because a photograph that overflows its page is cropped
// silently - the PDF looks deliberate, and the missing strip is only noticed by
// whoever needed what was on it.
qreal scaleFor(const QSize &imageSize, const QSizeF &pageSize);

// Where the image is drawn, centred on the page.
QPointF originFor(const QSize &imageSize, const QSizeF &pageSize);

// Writes the file: the photo, then `text`.
//
// `text` is what the result page shows, which is the amended text when anything
// has been corrected - that is the entire reason it is passed in rather than
// taken from the words, and the bug the old version had.
//
// Returns false if it could not be written.
bool write(const QString &path, const QImage &photo, const QString &text);

} // namespace PdfExport

#endif // PDFEXPORT_H
