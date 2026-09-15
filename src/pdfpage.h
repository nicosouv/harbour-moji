#ifndef PDFPAGE_H
#define PDFPAGE_H

#include <QSizeF>
#include <QString>

// Deciding how to rasterise a PDF page for recognition.
//
// No Poppler in here. The library call is three lines and lives in pdfrender.cpp;
// what is worth testing is the number handed to it, because it is the one thing
// that quietly decides whether the text comes out at all.
//
// A PDF page has no pixels and no resolution - it has a size in points, 72 to the
// inch. Rendering is a choice of DPI, and both ends of that choice fail:
//
//   too low  - Tesseract is trained near 300 DPI and falls off steeply below it.
//              An A4 page at 72 DPI is 595 pixels across, and body text at that
//              size is four pixels tall. It reads as nothing.
//   too high - an A4 page at 600 DPI is 5000x7000, which as the ARGB32 QImage
//              Poppler returns is 140MB. On a phone that is how the process gets
//              killed, and it buys nothing: ImagePrep::prepare scales anything
//              over MaxEdge straight back down again.
//
// So: aim at 300, and come down only far enough to stay inside the budget that
// ImagePrep is going to impose anyway.
namespace PdfPage {

// What Tesseract's models were trained near.
const qreal PreferredDpi = 300.0;

// The resolution to render a page of this size at, in DPI.
//
// `sizePoints` is the page's size in PostScript points, which is what Poppler
// reports. `maxEdge` is the longest edge the result may have - pass
// ImagePrep::MaxEdge, so the render lands at the size recognition wants instead
// of being made large and then thrown away.
//
// The budget is absolute and there is no floor under the answer. A floor was
// written first, on the reasoning that a huge page should not be rendered so
// small that nothing on it is legible - and a test found that it contradicted the
// only property worth guaranteeing here: an A0 poster at that floor is 3370
// pixels, which is over budget, and a plan ten times larger is over it by a
// factor of twelve. Rendering a page that large at 9 DPI does produce nothing
// legible, and that is the honest outcome: the recogniser reads nothing and says
// so, which is better than the process being killed.
qreal resolutionFor(const QSizeF &sizePoints, int maxEdge);

// Whether a file looks like a PDF, from its first bytes rather than its name.
//
// A picker hands back whatever the user chose, and "%PDF-" is one read. Trusting
// the extension means handing a renamed JPEG to Poppler and reporting "this PDF
// is damaged" about a file that is not a PDF at all.
bool looksLikePdf(const QString &path);

} // namespace PdfPage

#endif // PDFPAGE_H
