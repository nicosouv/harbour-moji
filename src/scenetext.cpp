#include "scenetext.h"

#include <QStack>

#include <algorithm>

namespace SceneText {

namespace {

// One blob: its extent, how many pixels it has, and the probability piled up over
// them. Kept as sums so the mean costs nothing at the end.
struct Blob
{
    int left = 0;
    int top = 0;
    int right = 0;
    int bottom = 0;
    int pixels = 0;
    double total = 0.0;
};

// The blob containing (x, y), marking every pixel of it as taken.
//
// Flood filled from an explicit stack rather than by recursion: a detector runs
// at something like 640x640, one blob can be most of that, and a recursive fill
// over a quarter of a million pixels is a stack overflow rather than a slow
// function. Four-connected, because eight would join two words that touch at a
// corner - which is the very thing DBNet's shrunk training set exists to avoid.
Blob fill(const Map &map, int x0, int y0, QVector<bool> &taken)
{
    Blob blob;
    blob.left = blob.right = x0;
    blob.top = blob.bottom = y0;

    QStack<QPoint> pending;
    pending.push(QPoint(x0, y0));
    taken[qint64(y0) * map.width + x0] = true;

    while (!pending.isEmpty()) {
        const QPoint at = pending.pop();
        const int x = at.x();
        const int y = at.y();

        const float value = map.at(x, y);
        blob.pixels += 1;
        blob.total += value;
        blob.left = qMin(blob.left, x);
        blob.right = qMax(blob.right, x);
        blob.top = qMin(blob.top, y);
        blob.bottom = qMax(blob.bottom, y);

        const int dx[4] = { 1, -1, 0, 0 };
        const int dy[4] = { 0, 0, 1, -1 };
        for (int i = 0; i < 4; ++i) {
            const int nx = x + dx[i];
            const int ny = y + dy[i];
            if (nx < 0 || ny < 0 || nx >= map.width || ny >= map.height) {
                continue;
            }
            const qint64 index = qint64(ny) * map.width + nx;
            if (taken[index] || map.at(nx, ny) <= MapThreshold) {
                continue;
            }
            taken[index] = true;
            pending.push(QPoint(nx, ny));
        }
    }

    return blob;
}

// The box grown back to the size of the text that produced it.
//
// DBNet is trained against shrunk regions, so what it reports is reliably smaller
// than the words. The standard rule grows a polygon outwards by area*ratio over
// perimeter; for an axis-aligned box that is one distance applied to all four
// sides. Written as a qreal throughout and rounded once: doing it in integers
// loses a pixel on a short word, and a box a pixel short of a descender is
// exactly the failure redaction already taught this project about.
QRect unclipped(const QRect &box, const QSize &bounds)
{
    const qreal width = box.width();
    const qreal height = box.height();
    const qreal perimeter = 2.0 * (width + height);
    if (perimeter <= 0.0) {
        return box;
    }

    const int grow = qRound(width * height * Unclip / perimeter);

    QRect out = box.adjusted(-grow, -grow, grow, grow);
    return out.intersected(QRect(QPoint(0, 0), bounds));
}

} // namespace

QVector<Region> regionsIn(const Map &map)
{
    QVector<Region> regions;
    if (map.isNull()) {
        return regions;
    }

    QVector<bool> taken(qint64(map.width) * map.height, false);

    for (int y = 0; y < map.height; ++y) {
        for (int x = 0; x < map.width; ++x) {
            const qint64 index = qint64(y) * map.width + x;
            if (taken[index] || map.at(x, y) <= MapThreshold) {
                continue;
            }

            const Blob blob = fill(map, x, y, taken);

            const float score = blob.pixels > 0
                    ? float(blob.total / blob.pixels) : 0.0f;
            if (score < BoxThreshold) {
                continue;
            }

            const QRect tight(blob.left, blob.top,
                              blob.right - blob.left + 1,
                              blob.bottom - blob.top + 1);
            if (tight.width() < MinSide || tight.height() < MinSide) {
                continue;
            }

            Region region;
            region.box = unclipped(tight, QSize(map.width, map.height));
            region.score = score;
            regions.append(region);
        }
    }

    // Reading order. Two boxes are on the same line when they overlap vertically
    // by more than half the shorter one - which is the rule that keeps a word with
    // a tall capital on the same line as its neighbours, where comparing centres
    // or tops does not.
    std::sort(regions.begin(), regions.end(),
              [](const Region &a, const Region &b) {
        const int overlap = qMin(a.box.bottom(), b.box.bottom())
                          - qMax(a.box.top(), b.box.top());
        const int shorter = qMin(a.box.height(), b.box.height());
        if (overlap * 2 > shorter) {
            return a.box.left() < b.box.left();
        }
        return a.box.top() < b.box.top();
    });

    return regions;
}

QRect toSourceRect(const QRect &box, const QSize &mapSize, const QSize &sourceSize)
{
    if (mapSize.width() <= 0 || mapSize.height() <= 0) {
        return box;
    }

    const qreal sx = qreal(sourceSize.width()) / mapSize.width();
    const qreal sy = qreal(sourceSize.height()) / mapSize.height();

    // The edges are scaled, not the origin and the size: scaling a width
    // separately lets two boxes that touched on the map stop touching, and the
    // gap lands between two halves of one word.
    const int left = qRound(box.left() * sx);
    const int top = qRound(box.top() * sy);
    const int right = qRound((box.right() + 1) * sx) - 1;
    const int bottom = qRound((box.bottom() + 1) * sy) - 1;

    return QRect(QPoint(left, top), QPoint(qMax(left, right), qMax(top, bottom)));
}

} // namespace SceneText
