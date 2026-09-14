#ifndef IMAGEPREP_H
#define IMAGEPREP_H

#include <QImage>
#include <QLineF>
#include <QRect>
#include <QString>
#include <QTransform>
#include <QVector>

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

// Loads a file the way the photographer saw it.
//
// A phone camera does not rotate the pixels when the phone is held upright: it
// writes the sensor's landscape frame and tags it with an EXIF orientation. Qt
// does not apply that tag unless asked - QImage(path) and QML's Image both ignore
// it by default - so a portrait photo arrives lying on its side, and both the
// preview and the recogniser see it that way.
//
// Returns a null image if the file cannot be read.
QImage loadUpright(const QString &path);

// The image turned by a multiple of 90 degrees, for trying the page the other way
// up. Any other angle returns the image unchanged.
QImage rotated(const QImage &image, int degrees);

// A box measured on an image that was rotated by `degrees`, expressed back in the
// coordinates of the image before that rotation.
//
// This is the part that silently ruins an overlay: recognise a rotated copy, draw
// the boxes it reports on the upright photo, and every one of them is in the
// wrong place - plausibly enough to look like a calibration problem rather than a
// missing transform. `rotatedSize` is the size of the image the box was measured
// on.
QRect unrotateRect(const QRect &box, int degrees, const QSize &rotatedSize);

// The tilt of a page, in degrees, from the baselines the recogniser reported.
//
// A page is rarely photographed square. Quarter turns fix a page held sideways
// and do nothing at all for one held at eight degrees, which is the common case
// and costs a great deal of accuracy - Tesseract's line finder tolerates a little
// skew and gets steadily worse through it.
//
// The median is taken, not the mean: a single baseline drawn across two columns,
// or along a rule in a table, is wildly wrong and would drag an average with it.
// Positive means the text runs downhill to the right.
//
// Returns 0 when there is nothing to measure.
qreal skewAngle(const QVector<QLineF> &baselines);

// The image turned by an arbitrary angle, and the exact transform Qt used to do
// it. The transform is needed to get boxes back: Qt translates the result so the
// rotated image still starts at the origin, and that offset is not something to
// re-derive by hand - QImage::trueMatrix reports it.
struct Turned
{
    QImage image;
    QTransform transform;
};

Turned turnedBy(const QImage &image, qreal degrees);

// A box measured on a turned image, back in the coordinates of the image before
// the turn.
QRect untransformRect(const QRect &box, const QTransform &transform);

// Below this a page is straight enough that turning it costs more than it gains;
// above it, the tilt is more likely a mis-measurement than a page.
const qreal MinSkew = 0.75;
const qreal MaxSkew = 20.0;

// Black and white, thresholded against the local average rather than a single
// value for the whole image.
//
// This is the largest free improvement available to a photograph. Tesseract
// binarises internally with Otsu's method, which picks *one* threshold for the
// whole page - excellent for a flatbed scan and defeated by the shadow gradient
// that every hand-held photo has, where the same grey is paper on one side of the
// frame and ink on the other. Comparing each pixel with the average of the window
// around it instead removes the gradient by construction.
//
// Bradley and Roth's method, over an integral image, so the window average costs
// four lookups per pixel regardless of how large the window is.
//
// windowFraction is the window's width as a fraction of the image's; delta is how
// far below its neighbourhood a pixel must sit to count as ink. The defaults are
// the ones that paper recommends.
QImage binarised(const QImage &grey, qreal windowFraction = 0.125,
                 qreal delta = 0.15);

// How much of a black-and-white image is black, between 0 and 1.
qreal inkFraction(const QImage &bw);

// Above this, thresholding has not found text - it has found noise.
//
// A page of text is a few percent ink; the two document photographs measured
// here came out at 5.8% and 23.8%, the second being a dense spread with a
// photograph in the frame. The three night photographs came out at 41%, 45% and
// 57%, because in a dark frame every speck of sensor noise is darker than its
// neighbours and the local threshold faithfully marks all of it.
//
// The gap is wide and the consequence is not subtle: Tesseract called 189 of
// those specks words on the sign photograph, at 17% confidence, which was enough
// to beat the seven real words it found in the same photograph untouched.
const qreal MaxInk = 0.35;

// Thresholded, or left alone when thresholding it produced noise instead of text.
//
// Local thresholding is the largest free improvement available to a photograph of
// a page and the largest free way to ruin a photograph taken at night. It cannot
// be decided from the setting alone, because it is the same setting and the same
// user; it has to be decided from the image. inkFraction measures exactly the
// thing that goes wrong, so that is what decides it.
QImage binarisedIfItHelps(const QImage &grey);

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
