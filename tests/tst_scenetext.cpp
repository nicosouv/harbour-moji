#include <QtTest>

#include "scenetext.h"

using namespace SceneText;

// Turning a text detector's probability map into boxes.
//
// Nothing here needs the detector, which is the point: the decisions are all on
// this side of it - which pixels are text, how far a box is grown back, what is
// too small to be a word, and the scaling from the model's fixed input size to
// the photograph. That last one is the one that fails silently, the same way
// ImagePrep::toSourceRect does: the boxes land somewhere plausible and slightly
// wrong, which reads as a rendering quirk rather than a missing transform.
class TestSceneText : public QObject
{
    Q_OBJECT

private slots:
    void emptyMapFindsNothing();
    void oneBlobIsOneRegion();
    void twoBlobsStaySeparate();
    void wordsThatTouchDiagonallyStaySeparate();
    void faintBlobsAreDropped();
    void specksAreDropped();
    void boxIsGrownPastTheBlob();
    void growingStopsAtTheEdge();
    void regionsComeBackInReadingOrder();

    void toSourceRectIsIdentityAtTheSameSize();
    void toSourceRectScalesBothWays();
    void toSourceRectKeepsTouchingBoxesTouching();
};

namespace {

// A map with `high` inside the given rectangles and nothing outside.
struct Canvas
{
    int width;
    int height;
    QVector<float> values;

    Canvas(int w, int h) : width(w), height(h), values(w * h, 0.0f) {}

    void paint(const QRect &box, float value)
    {
        for (int y = box.top(); y <= box.bottom(); ++y) {
            for (int x = box.left(); x <= box.right(); ++x) {
                values[y * width + x] = value;
            }
        }
    }

    Map map() const { return Map { values.constData(), width, height }; }
};

} // namespace

void TestSceneText::emptyMapFindsNothing()
{
    QVERIFY(regionsIn(Map()).isEmpty());

    Canvas blank(20, 20);
    QVERIFY(regionsIn(blank.map()).isEmpty());
}

void TestSceneText::oneBlobIsOneRegion()
{
    Canvas c(60, 60);
    c.paint(QRect(10, 10, 20, 8), 0.9f);

    const QVector<Region> found = regionsIn(c.map());
    QCOMPARE(found.size(), 1);
    QVERIFY(qAbs(found.first().score - 0.9f) < 1e-5f);

    // Grown, so it contains the blob rather than equalling it.
    QVERIFY(found.first().box.contains(QRect(10, 10, 20, 8)));
}

void TestSceneText::twoBlobsStaySeparate()
{
    Canvas c(80, 40);
    c.paint(QRect(5, 10, 10, 10), 0.9f);
    c.paint(QRect(60, 10, 10, 10), 0.9f);

    QCOMPARE(regionsIn(c.map()).size(), 2);
}

void TestSceneText::wordsThatTouchDiagonallyStaySeparate()
{
    // Four-connected, not eight. Two words whose corners meet are two words, and
    // joining them is exactly what DBNet's shrunk training set exists to avoid -
    // so the fill must not reach across a diagonal.
    Canvas c(40, 40);
    c.paint(QRect(5, 5, 10, 10), 0.9f);
    c.paint(QRect(15, 15, 10, 10), 0.9f);

    QCOMPARE(regionsIn(c.map()).size(), 2);
}

void TestSceneText::faintBlobsAreDropped()
{
    // Above the pixel threshold, below the box threshold: the detector saw
    // something and does not believe it.
    Canvas c(40, 40);
    c.paint(QRect(10, 10, 10, 10), 0.35f);

    QVERIFY(regionsIn(c.map()).isEmpty());
}

void TestSceneText::specksAreDropped()
{
    // Two pixels of noise, at full confidence. A night photograph is full of them.
    Canvas c(40, 40);
    c.paint(QRect(10, 10, 2, 2), 0.99f);

    QVERIFY(regionsIn(c.map()).isEmpty());
}

void TestSceneText::boxIsGrownPastTheBlob()
{
    // The rule is area * Unclip / perimeter, applied to all four sides. For a
    // 20x8 blob: 160 * 1.5 / 56 = 4.28, which rounds to 4.
    Canvas c(100, 100);
    c.paint(QRect(40, 40, 20, 8), 0.9f);

    const QVector<Region> found = regionsIn(c.map());
    QCOMPARE(found.size(), 1);
    QCOMPARE(found.first().box, QRect(36, 36, 28, 16));
}

void TestSceneText::growingStopsAtTheEdge()
{
    // Text that runs to the edge of the frame is the normal case for a sign
    // photographed close, and a box outside the map would index outside the
    // photograph when it is scaled back.
    Canvas c(40, 40);
    c.paint(QRect(0, 0, 12, 12), 0.9f);

    const QVector<Region> found = regionsIn(c.map());
    QCOMPARE(found.size(), 1);
    QCOMPARE(found.first().box.left(), 0);
    QCOMPARE(found.first().box.top(), 0);
    QVERIFY(found.first().box.right() < 40);
}

void TestSceneText::regionsComeBackInReadingOrder()
{
    // Painted bottom-first and right-first, so an unsorted answer would come back
    // in the wrong order. The two on the top line share a band and must come out
    // left then right; the one below comes last whatever its x is.
    Canvas c(200, 100);
    c.paint(QRect(10, 60, 30, 10), 0.9f);    // second line
    c.paint(QRect(120, 10, 30, 10), 0.9f);   // first line, right
    c.paint(QRect(10, 12, 30, 10), 0.9f);    // first line, left - slightly lower

    const QVector<Region> found = regionsIn(c.map());
    QCOMPARE(found.size(), 3);
    QVERIFY(found.at(0).box.left() < found.at(1).box.left());
    QVERIFY(found.at(0).box.top() < found.at(2).box.top());
    QVERIFY(found.at(1).box.top() < found.at(2).box.top());
}

void TestSceneText::toSourceRectIsIdentityAtTheSameSize()
{
    const QRect box(10, 20, 30, 40);
    QCOMPARE(toSourceRect(box, QSize(100, 100), QSize(100, 100)), box);
}

void TestSceneText::toSourceRectScalesBothWays()
{
    // A detector runs at a fixed size and a photograph is not that shape, so the
    // two axes scale by different amounts. Getting that wrong puts every box on
    // the right words horizontally and the wrong ones vertically.
    const QRect box(10, 10, 20, 20);
    const QRect out = toSourceRect(box, QSize(100, 50), QSize(400, 400));

    QCOMPARE(out.left(), 40);
    QCOMPARE(out.top(), 80);
    QCOMPARE(out.width(), 80);
    QCOMPARE(out.height(), 160);
}

void TestSceneText::toSourceRectKeepsTouchingBoxesTouching()
{
    // Two boxes that abut on the map must still abut on the photo. Scaling the
    // origin and the size separately opens a gap, and the gap lands between two
    // halves of one word.
    const QRect left(0, 0, 10, 10);
    const QRect right(10, 0, 10, 10);

    const QSize map(20, 20);
    const QSize source(133, 133);

    const QRect a = toSourceRect(left, map, source);
    const QRect b = toSourceRect(right, map, source);

    QCOMPARE(b.left(), a.right() + 1);
}

QTEST_APPLESS_MAIN(TestSceneText)
#include "tst_scenetext.moc"
