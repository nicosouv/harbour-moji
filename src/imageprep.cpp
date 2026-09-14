#include "imageprep.h"

#include <QImageReader>
#include <QTransform>

#include <algorithm>

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

QImage binarised(const QImage &grey, qreal windowFraction, qreal delta)
{
    if (grey.isNull() || grey.format() != QImage::Format_Grayscale8) {
        return grey;
    }

    const int width = grey.width();
    const int height = grey.height();

    // Sums of every pixel above and to the left, so the total of any rectangle is
    // four lookups. 64-bit because a 2400x1800 page of white is already 10^9.
    QVector<qint64> integral((width + 1) * (height + 1), 0);

    for (int y = 0; y < height; ++y) {
        const uchar *row = grey.constScanLine(y);
        qint64 rowSum = 0;
        for (int x = 0; x < width; ++x) {
            rowSum += row[x];
            integral[(y + 1) * (width + 1) + (x + 1)] =
                integral[y * (width + 1) + (x + 1)] + rowSum;
        }
    }

    int window = qRound(width * windowFraction);
    window = qMax(3, window | 1);          // odd, so it has a centre
    const int half = window / 2;

    QImage out(width, height, QImage::Format_Grayscale8);

    for (int y = 0; y < height; ++y) {
        const uchar *row = grey.constScanLine(y);
        uchar *outRow = out.scanLine(y);

        const int y1 = qMax(0, y - half);
        const int y2 = qMin(height - 1, y + half);

        for (int x = 0; x < width; ++x) {
            const int x1 = qMax(0, x - half);
            const int x2 = qMin(width - 1, x + half);

            const qint64 area = qint64(x2 - x1 + 1) * (y2 - y1 + 1);
            const qint64 sum = integral[(y2 + 1) * (width + 1) + (x2 + 1)]
                             - integral[y1 * (width + 1) + (x2 + 1)]
                             - integral[(y2 + 1) * (width + 1) + x1]
                             + integral[y1 * (width + 1) + x1];

            // Ink if the pixel is meaningfully darker than what surrounds it.
            const qint64 scaled = qint64(row[x]) * area;
            outRow[x] = (scaled < sum * (1.0 - delta)) ? 0 : 255;
        }
    }

    return out;
}

qreal inkFraction(const QImage &bw)
{
    if (bw.isNull() || bw.format() != QImage::Format_Grayscale8) {
        return 0.0;
    }

    qint64 ink = 0;
    for (int y = 0; y < bw.height(); ++y) {
        const uchar *row = bw.constScanLine(y);
        for (int x = 0; x < bw.width(); ++x) {
            // binarised() writes 0 or 255 and nothing between, so this is a
            // count rather than a threshold.
            if (row[x] == 0) {
                ++ink;
            }
        }
    }

    return qreal(ink) / (qreal(bw.width()) * bw.height());
}

QImage binarisedIfItHelps(const QImage &grey)
{
    const QImage bw = binarised(grey);
    if (bw.isNull() || inkFraction(bw) > MaxInk) {
        return grey;
    }
    return bw;
}

qreal skewAngle(const QVector<QLineF> &baselines)
{
    QVector<qreal> angles;
    angles.reserve(baselines.size());

    for (const QLineF &line : baselines) {
        // Very short baselines are single words or stray marks; their angle is
        // mostly quantisation noise.
        if (line.length() < 40.0) {
            continue;
        }

        // QLineF measures anticlockwise from the x axis with y upwards, while an
        // image has y downwards, so the sign is flipped to make "downhill to the
        // right" positive.
        qreal angle = -line.angle();
        while (angle <= -90.0) {
            angle += 180.0;
        }
        while (angle > 90.0) {
            angle -= 180.0;
        }
        angles.append(angle);
    }

    if (angles.isEmpty()) {
        return 0.0;
    }

    std::sort(angles.begin(), angles.end());
    const int middle = angles.size() / 2;
    return (angles.size() % 2 == 0)
               ? (angles.at(middle - 1) + angles.at(middle)) / 2.0
               : angles.at(middle);
}

Turned turnedBy(const QImage &image, qreal degrees)
{
    Turned turned;
    if (image.isNull()) {
        return turned;
    }

    QTransform rotation;
    rotation.rotate(degrees);

    // trueMatrix is the transform Qt will *actually* apply, including the
    // translation that keeps the result inside the origin. Rebuilding that offset
    // by hand is how an overlay ends up a few pixels adrift at one corner and
    // fine at the others.
    turned.transform = QImage::trueMatrix(rotation, image.width(), image.height());

    // Converted back to the format it arrived in. A quarter turn preserves
    // Format_Grayscale8, but an arbitrary angle has to interpolate and Qt returns
    // ARGB32_Premultiplied - four bytes per pixel where OcrEngine will tell
    // Tesseract there is one, so it would read every fourth byte and recognise
    // noise. Nothing would crash; the page would simply come out as gibberish.
    const QImage::Format wanted = image.format();
    QImage result = image.transformed(rotation, Qt::SmoothTransformation);
    turned.image = (result.format() == wanted) ? result
                                               : result.convertToFormat(wanted);
    return turned;
}

QRect untransformRect(const QRect &box, const QTransform &transform)
{
    bool invertible = false;
    const QTransform inverse = transform.inverted(&invertible);
    if (!invertible) {
        return box;
    }
    return inverse.mapRect(box);
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
