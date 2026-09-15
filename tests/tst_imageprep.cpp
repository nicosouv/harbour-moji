#include <QtTest>
#include <QPainter>
#include <QSet>
#include <QLineF>
#include <QTransform>
#include <QtMath>

#include <cmath>

#include "imageprep.h"

using namespace ImagePrep;

// The coordinate mapping, mostly.
//
// Everything else here is arithmetic that would be obvious if it broke - a photo
// the wrong size, a format Tesseract rejects. The mapping is not: if boxes come
// back in the scaled image's coordinates and are drawn on the original, the
// overlay lands *nearly* right, which reads as a rendering quirk rather than as a
// bug and can survive a long time.
class TestImagePrep : public QObject
{
    Q_OBJECT

private:
    static QImage photo(int width, int height)
    {
        QImage image(width, height, QImage::Format_RGB32);
        image.fill(Qt::white);
        return image;
    }

private slots:
    void scaleForLeavesSmallImagesAlone();
    void scaleForShrinksByTheLongEdge();
    void scaleForHandlesEmpty();

    void prepareProducesGreyscale();
    void prepareLeavesSmallImagesAtFullSize();
    void prepareCapsTheLongEdge();
    void preparePreservesAspectRatio();
    void prepareReportsTheRealisedScale();
    void prepareRejectsNull();

    void toSourceRectIsIdentityAtScaleOne();
    void toSourceRectUndoesTheScale();
    void toSourceRectKeepsTouchingBoxesTouching();
    void roundTripLandsWithinAPixel();

    void flattenedSizeTakesTheLongerEdges();
    void quadIsRefusedWhenItIsNotOne();
    void flatteningARectangleChangesNothingMuch();
    void flatteningPullsATrapezoidStraight();
    void flatteningRefusesRubbish();


    void inkFractionCountsTheBlack();
    void binarisingHelpsAPage();
    void binarisingIsRefusedOnNoise();

    void loadUprightAppliesTheExifTag();
    void rotatedTurnsTheImage();
    void rotatedIgnoresOddAngles();
    void unrotateRectIsIdentityAtZero();
    void unrotateRectUndoesNinety();
    void unrotateRectUndoesTwoSeventy();
    void unrotateRectRoundTripsEveryQuarterTurn();

    void skewAngleIsZeroForLevelText();
    void skewAngleReadsATilt();
    void skewAngleIgnoresShortBaselines();
    void skewAngleTakesTheMedianNotTheMean();
    void skewAngleHandlesNothingToMeasure();
    void turnedByRoundTripsABox();
    void turnedByLeavesTheImageUsable();

    void binarisedProducesOnlyBlackAndWhite();
    void binarisedKeepsTextOnAnEvenBackground();
    void binarisedSurvivesALightingGradient();
    void binarisedLeavesOtherFormatsAlone();
};

void TestImagePrep::scaleForLeavesSmallImagesAlone()
{
    // Never scaled up: a small photo is small because it is poor, and enlarging
    // it invents detail the recogniser reads as texture.
    QCOMPARE(scaleFor(QSize(800, 600), 2400), 1.0);
    QCOMPARE(scaleFor(QSize(2400, 1000), 2400), 1.0);
}

void TestImagePrep::scaleForShrinksByTheLongEdge()
{
    // Portrait: the height decides.
    QCOMPARE(scaleFor(QSize(3000, 4800), 2400), 0.5);
    // Landscape: the width does.
    QCOMPARE(scaleFor(QSize(4800, 3000), 2400), 0.5);
}

void TestImagePrep::scaleForHandlesEmpty()
{
    QCOMPARE(scaleFor(QSize(), 2400), 1.0);
    QCOMPARE(scaleFor(QSize(100, 100), 0), 1.0);
}

void TestImagePrep::prepareProducesGreyscale()
{
    // One byte per pixel, which is what OcrEngine passes to SetImage. Handing
    // Tesseract a 4-byte ARGB buffer while telling it 1 would not crash - it
    // would read every fourth byte and recognise noise.
    const Prepared prepared = prepare(photo(400, 300));
    QCOMPARE(prepared.image.format(), QImage::Format_Grayscale8);
}

void TestImagePrep::prepareLeavesSmallImagesAtFullSize()
{
    const Prepared prepared = prepare(photo(800, 600));
    QCOMPARE(prepared.image.size(), QSize(800, 600));
    QCOMPARE(prepared.scale, 1.0);
}

void TestImagePrep::prepareCapsTheLongEdge()
{
    const Prepared prepared = prepare(photo(4000, 3000), 2400);
    QCOMPARE(qMax(prepared.image.width(), prepared.image.height()), 2400);
}

void TestImagePrep::preparePreservesAspectRatio()
{
    const Prepared prepared = prepare(photo(4000, 3000), 2400);
    const qreal before = 4000.0 / 3000.0;
    const qreal after = qreal(prepared.image.width()) / prepared.image.height();
    QVERIFY(qAbs(before - after) < 0.01);
}

void TestImagePrep::prepareReportsTheRealisedScale()
{
    // 1000 / 3 is not an integer, so the width rounds and the requested scale is
    // not quite the one that happened. What comes back has to be the one that
    // happened, or the mapping is half a pixel adrift at the far edge.
    const Prepared prepared = prepare(photo(3000, 1000), 1000);
    const qreal realised = qreal(prepared.image.width()) / 3000.0;
    QVERIFY(qAbs(prepared.scale - realised) < 1e-9);
}

void TestImagePrep::prepareRejectsNull()
{
    const Prepared prepared = prepare(QImage());
    QVERIFY(prepared.isNull());
}

void TestImagePrep::toSourceRectIsIdentityAtScaleOne()
{
    const QRect box(10, 20, 30, 40);
    QCOMPARE(toSourceRect(box, 1.0), box);
}

void TestImagePrep::toSourceRectUndoesTheScale()
{
    // Halved on the way in, so doubled on the way back.
    QCOMPARE(toSourceRect(QRect(50, 100, 20, 10), 0.5), QRect(100, 200, 40, 20));
}

void TestImagePrep::toSourceRectKeepsTouchingBoxesTouching()
{
    // Two words whose boxes abut in the scaled image must still abut in the
    // source. Scaling width separately from x lets them drift apart by a pixel,
    // which shows on screen as a seam through a word.
    const QRect first = toSourceRect(QRect(10, 0, 7, 5), 0.37);
    const QRect second = toSourceRect(QRect(17, 0, 7, 5), 0.37);
    QCOMPARE(second.left(), first.right() + 1);
}

void TestImagePrep::roundTripLandsWithinAPixel()
{
    const qreal scale = scaleFor(QSize(4000, 3000), 2400);

    // A box measured on the scaled image, taken back to the source.
    const QRect scaled(600, 450, 120, 30);
    const QRect source = toSourceRect(scaled, scale);

    QVERIFY(qAbs(source.x() - scaled.x() / scale) <= 1);
    QVERIFY(qAbs(source.y() - scaled.y() / scale) <= 1);
    QVERIFY(qAbs(source.width() - scaled.width() / scale) <= 1);
    QVERIFY(qAbs(source.height() - scaled.height() / scale) <= 1);
}

void TestImagePrep::loadUprightAppliesTheExifTag()
{
    // The fixture is stored 200x100 with an EXIF orientation of 6, which is what
    // a phone writes when it is held upright: the sensor's landscape frame plus a
    // note to turn it. Read correctly it is 100x200.
    const QString path = QFINDTESTDATA("fixtures/portrait_exif.jpg");
    QVERIFY2(!path.isEmpty(), "fixture missing");

    // What Qt does by default, and why the photo came out sideways.
    const QImage naive(path);
    QCOMPARE(naive.size(), QSize(200, 100));

    const QImage upright = loadUpright(path);
    QCOMPARE(upright.size(), QSize(100, 200));
}

void TestImagePrep::rotatedTurnsTheImage()
{
    const QImage landscape = photo(200, 100);
    QCOMPARE(rotated(landscape, 90).size(), QSize(100, 200));
    QCOMPARE(rotated(landscape, 270).size(), QSize(100, 200));
    QCOMPARE(rotated(landscape, 180).size(), QSize(200, 100));
    QCOMPARE(rotated(landscape, 0).size(), QSize(200, 100));
}

void TestImagePrep::rotatedIgnoresOddAngles()
{
    // Only quarter turns are meaningful here; anything else would resample the
    // whole photo for nothing.
    const QImage landscape = photo(200, 100);
    QCOMPARE(rotated(landscape, 45).size(), QSize(200, 100));
}

void TestImagePrep::unrotateRectIsIdentityAtZero()
{
    const QRect box(10, 20, 30, 40);
    QCOMPARE(unrotateRect(box, 0, QSize(200, 100)), box);
}

void TestImagePrep::unrotateRectUndoesNinety()
{
    // A 200x100 image turned +90 is 100x200. A box at the top-left of the turned
    // image came from the bottom-left of the original.
    const QSize turned(100, 200);
    QCOMPARE(unrotateRect(QRect(0, 0, 10, 20), 90, turned), QRect(0, 90, 20, 10));
}

void TestImagePrep::unrotateRectUndoesTwoSeventy()
{
    const QSize turned(100, 200);
    QCOMPARE(unrotateRect(QRect(0, 0, 10, 20), 270, turned), QRect(180, 0, 20, 10));
}

void TestImagePrep::unrotateRectRoundTripsEveryQuarterTurn()
{
    // The property that actually matters: a box measured on the turned image and
    // mapped back must land on the same pixels, whichever quarter turn was used.
    const QImage original = photo(200, 100);
    const QRect inOriginal(30, 10, 40, 25);

    for (int angle : { 90, 180, 270 }) {
        const QImage turned = rotated(original, angle);

        // Where that box ends up after the turn, computed independently.
        QTransform t;
        t.rotate(angle);
        const QRect mapped = t.mapRect(QRect(inOriginal.x(), inOriginal.y(),
                                             inOriginal.width(), inOriginal.height()))
                                 .translated(angle == 90 ? original.height() : 0, 0)
                                 .translated(0, angle == 270 ? original.width() : 0);
        Q_UNUSED(mapped)

        // Round trip through the inverse: taking a box that we know came from
        // inOriginal, the inverse must return it.
        const QRect inTurned = unrotateRect(inOriginal, (360 - angle) % 360,
                                            original.size());
        const QRect back = unrotateRect(inTurned, angle, turned.size());
        QCOMPARE(back, inOriginal);
    }
}

// A baseline running left to right across a page, tilted by `degrees` downhill.
static QLineF baseline(qreal degrees, qreal length = 400.0)
{
    const qreal radians = qDegreesToRadians(degrees);
    return QLineF(0.0, 0.0,
                  length * std::cos(radians), length * std::sin(radians));
}

void TestImagePrep::skewAngleIsZeroForLevelText()
{
    QVector<QLineF> lines;
    for (int i = 0; i < 5; ++i) {
        lines.append(baseline(0.0));
    }
    QVERIFY(qAbs(skewAngle(lines)) < 0.01);
}

void TestImagePrep::skewAngleReadsATilt()
{
    QVector<QLineF> lines;
    for (int i = 0; i < 5; ++i) {
        lines.append(baseline(7.0));
    }
    QVERIFY2(qAbs(skewAngle(lines) - 7.0) < 0.1,
             qPrintable(QStringLiteral("got %1").arg(skewAngle(lines))));
}

void TestImagePrep::skewAngleIgnoresShortBaselines()
{
    QVector<QLineF> lines;
    // Three real lines at 5 degrees...
    for (int i = 0; i < 3; ++i) {
        lines.append(baseline(5.0));
    }
    // ...and a crop of stubs pointing anywhere, as a stray mark or a single
    // character would. They must not be allowed a vote.
    for (int i = 0; i < 10; ++i) {
        lines.append(baseline(70.0, 8.0));
    }
    QVERIFY2(qAbs(skewAngle(lines) - 5.0) < 0.1,
             qPrintable(QStringLiteral("got %1").arg(skewAngle(lines))));
}

void TestImagePrep::skewAngleTakesTheMedianNotTheMean()
{
    QVector<QLineF> lines;
    for (int i = 0; i < 5; ++i) {
        lines.append(baseline(3.0));
    }
    // One baseline drawn across two columns, or along a table rule: badly wrong,
    // and long enough to pass the length test. A mean would be dragged to ~11.
    lines.append(baseline(50.0, 900.0));

    QVERIFY2(qAbs(skewAngle(lines) - 3.0) < 0.1,
             qPrintable(QStringLiteral("got %1").arg(skewAngle(lines))));
}

void TestImagePrep::skewAngleHandlesNothingToMeasure()
{
    QCOMPARE(skewAngle(QVector<QLineF>()), 0.0);
}

void TestImagePrep::turnedByRoundTripsABox()
{
    // The property the overlay depends on: a box measured on the turned image,
    // mapped back, must land on the region it came from. Qt translates the
    // rotated result to keep it at the origin, and forgetting that offset puts
    // every box a little wrong in a way that looks like a calibration problem.
    const QImage original = photo(400, 300);
    const Turned turned = turnedBy(original, 7.0);

    QVERIFY(!turned.image.isNull());

    const QRect inOriginal(120, 90, 60, 20);
    const QRect inTurned = turned.transform.mapRect(inOriginal);
    const QRect back = untransformRect(inTurned, turned.transform);

    // mapRect of a rotated rectangle returns its bounding box, so the round trip
    // grows slightly; it must still be centred on where it started.
    QVERIFY(qAbs(back.center().x() - inOriginal.center().x()) <= 2);
    QVERIFY(qAbs(back.center().y() - inOriginal.center().y()) <= 2);
    QVERIFY(back.contains(inOriginal.center()));
}

void TestImagePrep::turnedByLeavesTheImageUsable()
{
    // Greyscale must survive: OcrEngine tells Tesseract there is one byte per
    // pixel, and a rotation that quietly returned ARGB would have it read every
    // fourth byte and recognise noise.
    const Prepared prepared = prepare(photo(400, 300));
    const Turned turned = turnedBy(prepared.image, 6.0);

    QCOMPARE(turned.image.format(), QImage::Format_Grayscale8);
    // Turning makes the bounding box larger, never smaller.
    QVERIFY(turned.image.width() >= prepared.image.width());
}

// A page of small dark marks - characters, in effect - optionally lit unevenly:
// bright on one side, shadowed on the other, as every hand-held photo is.
//
// The marks are deliberately small against the window the threshold averages
// over. That is what real text is: a little ink on mostly paper. A solid band
// spanning the frame would defeat any local method, because a window inside it
// contains nothing but band and the band becomes its own background - true of
// Bradley's method and of every other one, and not a case that text presents.
static QImage page(bool gradient)
{
    const int w = 300, h = 200;
    QImage image(w, h, QImage::Format_Grayscale8);

    for (int y = 0; y < h; ++y) {
        uchar *row = image.scanLine(y);
        for (int x = 0; x < w; ++x) {
            // Paper, then a gradient from well-lit to shadowed across the frame.
            int value = gradient ? 60 + (170 * x) / w : 230;

            // Glyph-sized marks: 6 wide, 12 tall, on a 16x20 grid, so any window
            // around one is mostly paper.
            const bool inRow = (y % 20) < 12 && y >= 20 && y < 180;
            const bool inColumn = (x % 16) < 6 && x >= 20 && x < 280;
            if (inRow && inColumn) {
                value = qMax(0, value - 90);
            }
            row[x] = uchar(qBound(0, value, 255));
        }
    }
    return image;
}

static int blackPixels(const QImage &image)
{
    int count = 0;
    for (int y = 0; y < image.height(); ++y) {
        const uchar *row = image.constScanLine(y);
        for (int x = 0; x < image.width(); ++x) {
            if (row[x] == 0) {
                ++count;
            }
        }
    }
    return count;
}

void TestImagePrep::binarisedProducesOnlyBlackAndWhite()
{
    const QImage out = binarised(page(false));
    QCOMPARE(out.format(), QImage::Format_Grayscale8);

    for (int y = 0; y < out.height(); ++y) {
        const uchar *row = out.constScanLine(y);
        for (int x = 0; x < out.width(); ++x) {
            QVERIFY2(row[x] == 0 || row[x] == 255, "every pixel must be ink or paper");
        }
    }
}

void TestImagePrep::binarisedKeepsTextOnAnEvenBackground()
{
    const int ink = blackPixels(binarised(page(false)));
    // Some of the page is ink, and nothing like all of it.
    QVERIFY2(ink > 1000, "the bars should survive");
    QVERIFY2(ink < 300 * 200 / 2, "the paper should not be turned to ink");
}

void TestImagePrep::binarisedSurvivesALightingGradient()
{
    // The point of the whole exercise. Under one global threshold - which is what
    // Tesseract applies internally - the shadowed end of this image is darker than
    // the lit end's ink, so a single cut either loses the text on one side or
    // floods the other. A local comparison cannot make that mistake.
    const int even = blackPixels(binarised(page(false)));
    const int lit = blackPixels(binarised(page(true)));

    QVERIFY2(lit > even / 2,
             "a lighting gradient must not swallow half the text");
    QVERIFY2(lit < even * 2,
             "a lighting gradient must not turn the shadowed side into ink");
}

void TestImagePrep::binarisedLeavesOtherFormatsAlone()
{
    // Handed something it cannot read one byte at a time, it must return it
    // untouched rather than produce nonsense.
    const QImage colour = photo(50, 50);
    QCOMPARE(binarised(colour).format(), colour.format());
}

namespace {

QPolygonF quad(const QPointF &tl, const QPointF &tr,
               const QPointF &br, const QPointF &bl)
{
    QPolygonF p;
    p << tl << tr << br << bl;
    return p;
}

} // namespace

void TestImagePrep::flattenedSizeTakesTheLongerEdges()
{
    // A page leaning away from the camera: the top edge is shorter than the
    // bottom. The result must be as wide as the *near* edge, which is the one
    // that was photographed at full resolution - sizing to the far edge throws
    // away detail that exists.
    const QPolygonF page = quad(QPointF(20, 0), QPointF(80, 0),
                                QPointF(100, 100), QPointF(0, 100));

    const QSize size = flattenedSize(page);
    QCOMPARE(size.width(), 100);   // the bottom edge, not the top's 60
    QVERIFY(size.height() >= 100);
}

void TestImagePrep::quadIsRefusedWhenItIsNotOne()
{
    const QSize bounds(200, 200);

    // Too few points.
    QPolygonF three;
    three << QPointF(0, 0) << QPointF(10, 0) << QPointF(10, 10);
    QVERIFY(!isUsableQuad(three, bounds));

    // Two corners in the same place: quadToQuad has no answer for that.
    QVERIFY(!isUsableQuad(quad(QPointF(0, 0), QPointF(0, 0),
                               QPointF(100, 100), QPointF(0, 100)), bounds));

    // A sliver. Flattening this asks for an image three pixels tall.
    QVERIFY(!isUsableQuad(quad(QPointF(0, 0), QPointF(180, 0),
                               QPointF(180, 3), QPointF(0, 3)), bounds));

    // Off the edge of the photograph.
    QVERIFY(!isUsableQuad(quad(QPointF(-40, 0), QPointF(180, 0),
                               QPointF(180, 180), QPointF(0, 180)), bounds));

    // And one that is fine.
    QVERIFY(isUsableQuad(quad(QPointF(10, 10), QPointF(190, 20),
                              QPointF(180, 190), QPointF(0, 180)), bounds));
}

void TestImagePrep::flatteningARectangleChangesNothingMuch()
{
    // The identity case. A quad that is already the whole rectangle must come
    // back as the same picture - if this drifts, every other case is drifting
    // too and no test would say so.
    QImage source(60, 40, QImage::Format_Grayscale8);
    source.fill(255);
    for (int x = 10; x < 50; ++x) {
        source.scanLine(20)[x] = 0;
    }

    const QImage out = flattened(source, quad(QPointF(0, 0), QPointF(60, 0),
                                              QPointF(60, 40), QPointF(0, 40)));
    QVERIFY(!out.isNull());
    QCOMPARE(out.format(), QImage::Format_Grayscale8);
    QCOMPARE(out.size(), QSize(60, 40));

    // The dark line is still dark, and still across the middle.
    QVERIFY(out.constScanLine(20)[30] < 128);
    QVERIFY(out.constScanLine(5)[30] > 128);
}

void TestImagePrep::flatteningPullsATrapezoidStraight()
{
    // A black bar drawn inside a trapezoid. After flattening, the bar should run
    // the full width of the result - which is the whole point: the converging
    // edges of a page photographed at an angle become parallel again.
    QImage source(200, 200, QImage::Format_Grayscale8);
    source.fill(255);

    // The page: narrow at the top, wide at the bottom.
    const QPolygonF page = quad(QPointF(60, 20), QPointF(140, 20),
                                QPointF(190, 180), QPointF(10, 180));

    // Fill the quad black so there is something with the quad's own shape.
    QPainter painter(&source);
    painter.setBrush(Qt::black);
    painter.setPen(Qt::NoPen);
    painter.drawPolygon(page);
    painter.end();

    const QImage out = flattened(source, page);
    QVERIFY(!out.isNull());

    // The whole result should now be the page, corner to corner - a trapezoid
    // pulled to a rectangle leaves no white wedges at the sides.
    const int inset = 4;
    QVERIFY(out.constScanLine(inset)[inset] < 128);
    QVERIFY(out.constScanLine(inset)[out.width() - 1 - inset] < 128);
    QVERIFY(out.constScanLine(out.height() - 1 - inset)[inset] < 128);
    QVERIFY(out.constScanLine(out.height() - 1 - inset)[out.width() - 1 - inset] < 128);
}

void TestImagePrep::flatteningRefusesRubbish()
{
    QImage source(100, 100, QImage::Format_Grayscale8);
    source.fill(255);

    QVERIFY(flattened(QImage(), quad(QPointF(0, 0), QPointF(10, 0),
                                     QPointF(10, 10), QPointF(0, 10))).isNull());
    QVERIFY(flattened(source, QPolygonF()).isNull());
    QVERIFY(flattened(source, quad(QPointF(0, 0), QPointF(0, 0),
                                   QPointF(0, 0), QPointF(0, 0))).isNull());
}

void TestImagePrep::inkFractionCountsTheBlack()
{
    QImage bw(10, 10, QImage::Format_Grayscale8);
    bw.fill(255);
    QCOMPARE(inkFraction(bw), 0.0);

    // A tenth of it black.
    for (int x = 0; x < 10; ++x) {
        bw.scanLine(0)[x] = 0;
    }
    QVERIFY(qAbs(inkFraction(bw) - 0.1) < 1e-9);

    bw.fill(0);
    QCOMPARE(inkFraction(bw), 1.0);
}

void TestImagePrep::binarisingHelpsAPage()
{
    // A page: pale, with dark marks on a little of it. Thresholding this is the
    // whole reason the function exists, so it has to come back thresholded.
    QImage page(200, 200, QImage::Format_Grayscale8);
    page.fill(230);
    for (int y = 20; y < 40; ++y) {
        for (int x = 20; x < 180; ++x) {
            page.scanLine(y)[x] = 30;
        }
    }

    const QImage out = binarisedIfItHelps(page);
    QCOMPARE(out.format(), QImage::Format_Grayscale8);

    // Two levels and nothing between: it was thresholded, not returned.
    QSet<int> levels;
    for (int y = 0; y < out.height(); ++y) {
        for (int x = 0; x < out.width(); ++x) {
            levels.insert(out.constScanLine(y)[x]);
        }
    }
    QVERIFY(levels.contains(0));
    QVERIFY(levels.contains(255));
    QCOMPARE(levels.size(), 2);
    QVERIFY(inkFraction(out) <= MaxInk);
}

void TestImagePrep::binarisingIsRefusedOnNoise()
{
    // A photograph taken at night: dark everywhere, with sensor noise on top and
    // no page in it. Thresholding against the local average marks roughly half
    // of that as ink, and Tesseract reads a couple of hundred specks as words -
    // which used to beat the handful of real words in the untouched image,
    // because conf*sqrt(n) rewards a long list of things it does not believe.
    //
    // Deterministic noise: a test that fails one run in fifty is worse than none.
    QImage night(200, 200, QImage::Format_Grayscale8);
    quint32 seed = 12345;
    for (int y = 0; y < night.height(); ++y) {
        uchar *row = night.scanLine(y);
        for (int x = 0; x < night.width(); ++x) {
            seed = seed * 1103515245u + 12345u;
            row[x] = uchar(20 + ((seed >> 16) & 0x3F));
        }
    }

    QVERIFY(inkFraction(binarised(night)) > MaxInk);

    // So the image comes back as it went in, pixel for pixel.
    const QImage out = binarisedIfItHelps(night);
    QCOMPARE(out, night);
}

QTEST_APPLESS_MAIN(TestImagePrep)
#include "tst_imageprep.moc"
