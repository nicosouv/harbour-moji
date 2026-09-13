#include "imageprep.h"

#include <QImageReader>
#include <QTransform>

namespace ImagePrep {

QImage loadUpright(const QString &path)
{
    QImageReader reader(path);
    // The whole point. Without it a portrait photo comes back landscape, because
    // the camera stored it landscape and only tagged it.
    reader.setAutoTransform(true);
    return reader.read();
}

QImage rotated(const QImage &image, int degrees)
{
    const int turns = ((degrees % 360) + 360) % 360;
    if (turns == 0 || image.isNull()) {
        return image;
    }
    if (turns != 90 && turns != 180 && turns != 270) {
        return image;
    }
    return image.transformed(QTransform().rotate(turns), Qt::SmoothTransformation);
}

QRect unrotateRect(const QRect &box, int degrees, const QSize &rotatedSize)
{
    const int turns = ((degrees % 360) + 360) % 360;
    if (turns == 0) {
        return box;
    }

    const int rw = rotatedSize.width();
    const int rh = rotatedSize.height();

    // Worked in edges, and stated per case rather than through a QTransform: the
    // inverse of a 90-degree turn is easy to write and very easy to get subtly
    // backwards, and a test can only pin it if it is written out.
    switch (turns) {
    case 90:
        // The original was turned +90 (clockwise) to make the rotated image, so
        // the original's width is the rotated height.
        return QRect(box.y(), rw - box.x() - box.width(),
                     box.height(), box.width());
    case 180:
        return QRect(rw - box.x() - box.width(), rh - box.y() - box.height(),
                     box.width(), box.height());
    case 270:
        return QRect(rh - box.y() - box.height(), box.x(),
                     box.height(), box.width());
    default:
        return box;
    }
}

qreal scaleFor(const QSize &size, int maxEdge)
{
    if (size.isEmpty() || maxEdge <= 0) {
        return 1.0;
    }

    const int longEdge = qMax(size.width(), size.height());
    if (longEdge <= maxEdge) {
        // Never scaled up. A small photo is small because it is low quality, and
        // enlarging it invents detail the recogniser then reads as texture.
        return 1.0;
    }

    return static_cast<qreal>(maxEdge) / longEdge;
}

Prepared prepare(const QImage &source, int maxEdge)
{
    Prepared prepared;
    if (source.isNull()) {
        return prepared;
    }

    prepared.scale = scaleFor(source.size(), maxEdge);

    QImage working = source;
    if (prepared.scale < 1.0) {
        const QSize target(qRound(source.width() * prepared.scale),
                           qRound(source.height() * prepared.scale));
        working = source.scaled(target, Qt::IgnoreAspectRatio,
                                Qt::SmoothTransformation);

        // The rounding above means the realised scale is not exactly the
        // requested one. Recomputing from what actually came out keeps the
        // mapping back exact rather than half a pixel adrift at the right edge.
        prepared.scale = static_cast<qreal>(working.width()) / source.width();
    }

    // Greyscale last, so the smooth scale still has colour to average.
    prepared.image = working.convertToFormat(QImage::Format_Grayscale8);
    return prepared;
}

QRect toSourceRect(const QRect &box, qreal scale)
{
    if (scale <= 0.0 || qFuzzyCompare(scale, 1.0)) {
        return box;
    }

    const qreal inverse = 1.0 / scale;

    // Mapped as edges rather than as position-plus-size: scaling a width
    // separately from an x lets the right edge drift by a pixel against the next
    // box along, which shows up as a visible seam between two words that were
    // touching.
    const int left = qRound(box.left() * inverse);
    const int top = qRound(box.top() * inverse);
    const int right = qRound((box.right() + 1) * inverse) - 1;
    const int bottom = qRound((box.bottom() + 1) * inverse) - 1;

    return QRect(QPoint(left, top), QPoint(right, bottom));
}

} // namespace ImagePrep
