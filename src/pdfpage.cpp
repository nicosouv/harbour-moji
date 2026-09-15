#include "pdfpage.h"

#include <QFile>

namespace PdfPage {

qreal resolutionFor(const QSizeF &sizePoints, int maxEdge)
{
    const qreal longestPoints = qMax(sizePoints.width(), sizePoints.height());
    if (longestPoints <= 0.0 || maxEdge <= 0) {
        return PreferredDpi;
    }

    // What 300 DPI would produce, and whether that fits.
    const qreal atPreferred = longestPoints * PreferredDpi / 72.0;
    if (atPreferred <= qreal(maxEdge)) {
        return PreferredDpi;
    }

    // It does not, so render exactly to the budget. Not less: every pixel below
    // this is one Tesseract could have had for free. And not more, whatever the
    // page - see the header on why there is no floor here.
    return qreal(maxEdge) * 72.0 / longestPoints;
}

bool looksLikePdf(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    // "%PDF-" is required at the start of the file by the specification, and
    // every reader in practice allows a little junk before it.
    const QByteArray head = file.read(1024);
    return head.contains("%PDF-");
}

} // namespace PdfPage
