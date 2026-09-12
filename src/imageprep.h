#ifndef IMAGEPREP_H
#define IMAGEPREP_H

#include <QImage>
#include <QRect>

// Getting a camera photo into the shape Tesseract wants, and keeping the way
// back.
//
// Pure QImage work: no Tesseract, no OpenCV, so tests/ can reach it. That
// matters more than it sounds, because the one thing here that can be silently
// wrong is the coordinate mapping - if the image is scaled down before
// recognition, every box comes back in the scaled image's coordinates, and an
// overlay drawn from them lands in the wrong place on the photo. A test can pin
// that; an eye looking at a nearly-right overlay often cannot.
namespace ImagePrep {

struct Prepared
{
    QImage image;

    // What the source was multiplied by to get here, so boxes can be divided by
    // it to get back. 1.0 when nothing was scaled.
    qreal scale = 1.0;

    bool isNull() const { return image.isNull(); }
};

// A phone camera photo is far larger than recognition needs and costs time and
// memory in proportion. Above this on the long edge the image is scaled down.
//
// 2400 is chosen against what Tesseract wants rather than against what looks
// good: its models are trained near 300 DPI, which is about 2500 pixels across a
// sheet of A4. Feeding it a 12-megapixel original makes it slower without making
// it better, and on a phone it is also how the process gets itself killed - 12MP
// held as 8-bit grey is 12MB, but as the 32-bit ARGB QImage decodes to first, 48MB.
const int MaxEdge = 2400;

// Greyscale, no larger than maxEdge on its long side.
//
// Greyscale because Tesseract discards colour anyway, and one byte per pixel
// instead of four is the difference that decides whether a large photo fits.
Prepared prepare(const QImage &source, int maxEdge = MaxEdge);

// The scale prepare() would apply. Exposed so the mapping can be checked without
// doing the work.
qreal scaleFor(const QSize &size, int maxEdge = MaxEdge);

// A box from the prepared image, back in the source's coordinates.
QRect toSourceRect(const QRect &box, qreal scale);

} // namespace ImagePrep

#endif // IMAGEPREP_H
