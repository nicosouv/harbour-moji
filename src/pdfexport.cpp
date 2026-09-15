#include "pdfexport.h"

#include <QFont>
#include <QPainter>
#include <QPdfWriter>
#include <QTextDocument>
#include <QAbstractTextDocumentLayout>

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

bool write(const QString &path, const QImage &photo, const QString &text)
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

    // Page one: the photograph, as large as it goes.
    const qreal scale = scaleFor(photo.size(), pageSize);
    const QPointF origin = originFor(photo.size(), pageSize);
    painter.drawImage(QRectF(origin.x(), origin.y(),
                             photo.width() * scale, photo.height() * scale),
                      photo);

    if (!text.trimmed().isEmpty()) {
        // The text, flowed over as many pages as it takes.
        //
        // Laid out by QTextDocument rather than by drawing lines one at a time:
        // a page of recognised text wraps, and working out where to break it by
        // hand is how the last line of every page ends up half drawn over the
        // first line of the next.
        QTextDocument document;
        document.setDefaultFont(QFont(QStringLiteral("Sans"), 10));
        document.setPlainText(text);

        const qreal textWidth = pageSize.width() - 2 * Margin;
        const qreal textHeight = pageSize.height() - 2 * Margin;
        document.setPageSize(QSizeF(textWidth, textHeight));

        const int pages = document.pageCount();
        for (int i = 0; i < pages; ++i) {
            writer.newPage();

            painter.save();
            painter.translate(Margin, Margin);

            // The window is this page's slice of the document; everything outside
            // it is clipped, which is what stops the neighbouring pages' lines
            // bleeding into this one.
            const QRectF window(0, i * textHeight, textWidth, textHeight);
            painter.setClipRect(0, 0, textWidth, textHeight);
            painter.translate(0, -window.top());

            QAbstractTextDocumentLayout::PaintContext context;
            context.clip = window;
            context.palette.setColor(QPalette::Text, Qt::black);
            document.documentLayout()->draw(&painter, context);

            painter.restore();
        }
    }

    painter.end();
    qCDebug(lcMoji) << "wrote a PDF with the photo and"
                    << text.length() << "characters of text to" << path;
    return true;
}

} // namespace PdfExport
