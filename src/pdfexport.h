#ifndef PDFEXPORT_H
#define PDFEXPORT_H

#include <QImage>
#include <QPointF>
#include <QRect>
#include <QSizeF>
#include <QString>

#include "ocrresult.h"

// Writing the photo out as a PDF with the recognised text laid invisibly over it.
//
// The result opens anywhere, looks exactly like the photograph, and is fully
// searchable and selectable - which is what every scanner app promises and almost
// none deliver without a server. Nothing here needs one: the boxes are already in
// OcrResult, and a PDF is the one format where "a picture with text behind it" is
// a native idea rather than a trick.
//
// The text is drawn with a transparent pen. It is still emitted into the content
// stream as real text, so a reader finds it, selects it and copies it; it simply
// paints nothing over the image.
namespace PdfExport {

// A4 at 72 points per inch, which is what QPdfWriter's default resolution means.
const qreal PageWidth = 595.0;
const qreal PageHeight = 842.0;

// How many points one image pixel becomes, fitting the image on the page without
// cropping or distorting it.
//
// Pure, and tested, because it is the number everything else is expressed in: get
// it wrong and the invisible text sits somewhere other than the words it belongs
// to, which nobody would notice by looking - the page looks perfect - until a
// search highlights the wrong part of it.
qreal scaleFor(const QSize &imageSize, const QSizeF &pageSize);

// Where the image is drawn, centred on the page.
QPointF originFor(const QSize &imageSize, const QSizeF &pageSize);

// The point size that makes a word's invisible text the height of the box it was
// recognised in. Selecting a word in a reader then highlights the word, not a
// sliver of it or the line above.
qreal fontSizeForBox(const QRect &box, qreal scale);

// Where that text's baseline sits, in page coordinates.
//
// A box encloses the whole glyph including descenders, so the baseline is not its
// bottom edge - it is roughly four fifths down. Using the bottom puts every line
// slightly low, which is invisible until someone selects a paragraph and gets the
// line beneath it.
QPointF baselineFor(const QRect &box, qreal scale, const QPointF &origin);

// Writes the file. Returns false if it could not be written.
bool write(const QString &path, const QImage &photo, const OcrResult &result);

} // namespace PdfExport

#endif // PDFEXPORT_H
