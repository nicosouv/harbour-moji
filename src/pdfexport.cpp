#include "pdfexport.h"

#include <QFont>
#include <QPainter>
#include <QPdfWriter>

#include "logging.h"

namespace PdfExport {

qreal scaleFor(const QSize &imageSize, const QSizeF &pageSize)
{
    if (imageSize.isEmpty() || pageSize.isEmpty()) {
        return 1.0;
    }

    // Whichever dimension runs out first decides, so the whole photo fits.
    const qreal byWidth = pageSize.width() / imageSize.width();
    const qreal byHeight = pageSize.height() / imageSize.height();
    return qMin(byWidth, byHeight);
}

QPointF originFor(const QSize &imageSize, const QSizeF &pageSize)
{
    const qreal scale = scaleFor(imageSize, pageSize);
    return QPointF((pageSize.width() - imageSize.width() * scale) / 2.0,
                   (pageSize.height() - imageSize.height() * scale) / 2.0);
}

qreal fontSizeForBox(const QRect &box, qreal scale)
{
    // The cap height of a typeface is about 70% of its point size, and a word box
    // encloses roughly the cap height plus descenders. Sizing the font to the box
    // height directly makes the invisible text noticeably taller than the word,
    // and a reader's selection then spills into the lines around it.
    const qreal height = box.height() * scale * 0.8;
    return qMax(1.0, height);
}

QPointF baselineFor(const QRect &box, qreal scale, const QPointF &origin)
{
    // Four fifths down the box: descenders live below the baseline, so the bottom
    // edge is not it.
    return QPointF(origin.x() + box.x() * scale,
                   origin.y() + (box.y() + box.height() * 0.8) * scale);
}

bool write(const QString &path, const QImage &photo, const OcrResult &result)
{
    if (photo.isNull()) {
        return false;
    }

    QPdfWriter writer(path);
    writer.setPageSize(QPagedPaintDevice::A4);
    // 72 dpi, so one point is one unit and the geometry above needs no second
    // conversion.
    writer.setResolution(72);
    writer.setTitle(QStringLiteral("Moji OCR"));

    QPainter painter;
    if (!painter.begin(&writer)) {
        qCWarning(lcMoji) << "cannot write a PDF to" << path;
        return false;
    }

    const QSizeF pageSize(PageWidth, PageHeight);
    const qreal scale = scaleFor(photo.size(), pageSize);
    const QPointF origin = originFor(photo.size(), pageSize);

    painter.drawImage(QRectF(origin.x(), origin.y(),
                             photo.width() * scale, photo.height() * scale),
                      photo);

    // Transparent, not white and not zero-opacity: the text still goes into the
    // content stream as text, so a reader finds and selects it, but it paints
    // nothing at all over the photograph.
    painter.setPen(Qt::transparent);

    QFont font = painter.font();
    for (const OcrWord &word : result.words()) {
        if (word.text.isEmpty() || word.box.isEmpty()) {
            continue;
        }
        font.setPointSizeF(fontSizeForBox(word.box, scale));
        painter.setFont(font);
        painter.drawText(baselineFor(word.box, scale, origin), word.text);
    }

    painter.end();
    qCDebug(lcMoji) << "wrote a searchable PDF with" << result.count()
                    << "words to" << path;
    return true;
}

} // namespace PdfExport
