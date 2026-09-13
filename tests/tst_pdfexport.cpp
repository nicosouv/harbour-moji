#include <QtTest>

#include <QTemporaryDir>

#include "pdfexport.h"

using namespace PdfExport;

// Laying the recognised text invisibly over the photo.
//
// The geometry is what the tests are for. A PDF that is slightly wrong here looks
// perfect - it is the photograph - and only misbehaves when somebody searches it
// and the highlight lands on the wrong line. There is nothing to notice by
// looking, so it has to be pinned by arithmetic.
class TestPdfExport : public QObject
{
    Q_OBJECT

private:
    static QSizeF page() { return QSizeF(PageWidth, PageHeight); }

    static OcrResult oneWord(const QRect &box, const QString &text)
    {
        OcrResult result;
        OcrWord word;
        word.text = text;
        word.box = box;
        word.confidence = 90.0f;
        result.append(word);
        result.setImageSize(QSize(1000, 1400));
        return result;
    }

private slots:
    void scaleFitsAPortraitImage();
    void scaleFitsALandscapeImage();
    void scaleNeverCropsTheImage();
    void scaleHandlesNothing();

    void originCentresTheImage();
    void originIsZeroOnOneAxisWhenItFills();

    void fontSizeFollowsTheBoxHeight();
    void fontSizeIsNeverZero();

    void baselineSitsInsideTheBox();
    void baselineIsBelowTheBoxTop();

    void writeProducesAPdf();
    void writeRefusesANullImage();
};

void TestPdfExport::scaleFitsAPortraitImage()
{
    // Taller than A4's proportions, so the height decides.
    const qreal scale = scaleFor(QSize(1000, 2000), page());
    QVERIFY(qAbs(scale - PageHeight / 2000.0) < 1e-9);
}

void TestPdfExport::scaleFitsALandscapeImage()
{
    // Wider than A4, so the width decides.
    const qreal scale = scaleFor(QSize(2000, 1000), page());
    QVERIFY(qAbs(scale - PageWidth / 2000.0) < 1e-9);
}

void TestPdfExport::scaleNeverCropsTheImage()
{
    // Whatever the shape, the scaled image has to fit inside the page. Losing an
    // edge of somebody's document to a rounding choice is not a trade worth
    // making silently.
    const QVector<QSize> shapes {
        QSize(1000, 1400), QSize(4000, 3000), QSize(500, 5000), QSize(3000, 300)
    };
    for (const QSize &shape : shapes) {
        const qreal scale = scaleFor(shape, page());
        QVERIFY2(shape.width() * scale <= PageWidth + 1e-6,
                 qPrintable(QStringLiteral("too wide for %1x%2")
                                .arg(shape.width()).arg(shape.height())));
        QVERIFY2(shape.height() * scale <= PageHeight + 1e-6,
                 qPrintable(QStringLiteral("too tall for %1x%2")
                                .arg(shape.width()).arg(shape.height())));
    }
}

void TestPdfExport::scaleHandlesNothing()
{
    QCOMPARE(scaleFor(QSize(), page()), 1.0);
    QCOMPARE(scaleFor(QSize(100, 100), QSizeF()), 1.0);
}

void TestPdfExport::originCentresTheImage()
{
    const QSize image(1000, 1000);   // square: letterboxed on the long axis
    const QPointF origin = originFor(image, page());
    const qreal scale = scaleFor(image, page());

    const qreal leftGap = origin.x();
    const qreal rightGap = PageWidth - (origin.x() + image.width() * scale);
    QVERIFY(qAbs(leftGap - rightGap) < 1e-6);
}

void TestPdfExport::originIsZeroOnOneAxisWhenItFills()
{
    // A4's own proportions: it fills the width exactly, so there is no margin
    // there and the offset must be zero rather than a hair off it.
    const QSize image(595, 842);
    const QPointF origin = originFor(image, page());
    QVERIFY(qAbs(origin.x()) < 1e-6);
    QVERIFY(qAbs(origin.y()) < 1e-6);
}

void TestPdfExport::fontSizeFollowsTheBoxHeight()
{
    // Twice the box, twice the size: the invisible text has to track the words it
    // is standing in for, or selecting a heading picks up the body text.
    const qreal small = fontSizeForBox(QRect(0, 0, 100, 20), 1.0);
    const qreal large = fontSizeForBox(QRect(0, 0, 100, 40), 1.0);
    QVERIFY(qAbs(large - 2.0 * small) < 1e-6);
}

void TestPdfExport::fontSizeIsNeverZero()
{
    // A degenerate box from a stray mark must not produce a zero or negative
    // point size, which Qt refuses and which would drop the word silently.
    QVERIFY(fontSizeForBox(QRect(0, 0, 1, 0), 0.1) >= 1.0);
    QVERIFY(fontSizeForBox(QRect(), 1.0) >= 1.0);
}

void TestPdfExport::baselineSitsInsideTheBox()
{
    const QRect box(100, 200, 80, 30);
    const qreal scale = 0.5;
    const QPointF origin(10.0, 20.0);
    const QPointF baseline = baselineFor(box, scale, origin);

    const qreal top = origin.y() + box.top() * scale;
    const qreal bottom = origin.y() + box.bottom() * scale;

    QVERIFY(baseline.y() > top);
    QVERIFY(baseline.y() <= bottom + 1e-6);
    // Text starts at the left edge of the word it replaces.
    QVERIFY(qAbs(baseline.x() - (origin.x() + box.x() * scale)) < 1e-6);
}

void TestPdfExport::baselineIsBelowTheBoxTop()
{
    // Specifically not the bottom edge: descenders hang below the baseline, so
    // putting it at the bottom sets every line a little low and a paragraph
    // selection picks up the line underneath.
    const QRect box(0, 0, 100, 100);
    const QPointF baseline = baselineFor(box, 1.0, QPointF(0, 0));
    QVERIFY(baseline.y() > 50.0);
    QVERIFY(baseline.y() < 100.0);
}

void TestPdfExport::writeProducesAPdf()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("out.pdf"));

    QImage photo(400, 600, QImage::Format_RGB32);
    photo.fill(Qt::white);

    QVERIFY(write(path, photo, oneWord(QRect(50, 100, 120, 24),
                                       QStringLiteral("Facture"))));

    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QByteArray head = file.read(5);
    QCOMPARE(head, QByteArray("%PDF-"));
    QVERIFY2(file.size() > 1000, "a PDF holding a photo should not be tiny");
}

void TestPdfExport::writeRefusesANullImage()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QVERIFY(!write(dir.filePath(QStringLiteral("none.pdf")), QImage(), OcrResult()));
}

QTEST_MAIN(TestPdfExport)
#include "tst_pdfexport.moc"
