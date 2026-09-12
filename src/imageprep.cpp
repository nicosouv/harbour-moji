#include "imageprep.h"

namespace ImagePrep {

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
