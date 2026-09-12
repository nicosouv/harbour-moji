#include <QtTest>

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

QTEST_APPLESS_MAIN(TestImagePrep)
#include "tst_imageprep.moc"
