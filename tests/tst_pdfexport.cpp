#include <QtTest>

#include <QTemporaryDir>

#include "pdfexport.h"

using namespace PdfExport;

// Writing the photograph and its text out as one PDF.
//
// The geometry that is still worth pinning is the fit: an image scaled by even a
// little too much is cropped by the page, and a cropped PDF looks entirely
// deliberate - the missing strip is noticed only by whoever needed what was on it.
//
// What is no longer tested, because it is no longer done: the invisible text layer
// positioned box by box over the photograph. That was replaced by plain text on
// its own pages, which carries the user's corrections and can be read by a person
// rather than only found by a search.
class TestPdfExport : public QObject
{
    Q_OBJECT

private:
    static QSizeF page() { return QSizeF(PageWidth, PageHeight); }

    static QImage photo(int width, int height)
    {
        QImage image(width, height, QImage::Format_RGB32);
        image.fill(Qt::white);
        return image;
    }

private slots:
    void scaleFitsAPortraitImage();
    void scaleFitsALandscapeImage();
    void scaleNeverCropsTheImage_data();
    void scaleNeverCropsTheImage();
    void scaleHandlesNothing();

    void originCentresTheImage();
    void originIsZeroForAnExactFit();

    void writesAFile();
    void writesWithoutAnyText();
    void refusesAnEmptyPhoto();
    void longTextRunsToSeveralPages();
};

void TestPdfExport::scaleFitsAPortraitImage()
{
    // Taller than it is wide, against a taller-than-wide page: the height decides.
    const qreal scale = scaleFor(QSize(1000, 2000), page());
    QVERIFY(qAbs(scale - PageHeight / 2000.0) < 1e-9);
}

void TestPdfExport::scaleFitsALandscapeImage()
{
    // Wider than the page is: the width decides.
    const qreal scale = scaleFor(QSize(2000, 1000), page());
    QVERIFY(qAbs(scale - PageWidth / 2000.0) < 1e-9);
}

void TestPdfExport::scaleNeverCropsTheImage_data()
{
    QTest::addColumn<QSize>("size");
    QTest::newRow("portrait photo") << QSize(3000, 4000);
    QTest::newRow("landscape photo") << QSize(4000, 3000);
    QTest::newRow("square") << QSize(2500, 2500);
    QTest::newRow("panorama") << QSize(8000, 1200);
    QTest::newRow("tall receipt") << QSize(900, 6000);
    QTest::newRow("smaller than the page") << QSize(100, 120);
}

void TestPdfExport::scaleNeverCropsTheImage()
{
    QFETCH(QSize, size);

    const qreal scale = scaleFor(size, page());

    // The property that protects the file: whatever the photograph's shape, both
    // of its sides land inside the page. A tolerance of a thousandth of a point,
    // because this is float arithmetic and not a promise about exact equality.
    QVERIFY2(size.width() * scale <= PageWidth + 1e-3,
             qPrintable(QStringLiteral("width %1").arg(size.width() * scale)));
    QVERIFY2(size.height() * scale <= PageHeight + 1e-3,
             qPrintable(QStringLiteral("height %1").arg(size.height() * scale)));
}

void TestPdfExport::scaleHandlesNothing()
{
    QCOMPARE(scaleFor(QSize(), page()), 1.0);
    QCOMPARE(scaleFor(QSize(100, 100), QSizeF()), 1.0);
}

void TestPdfExport::originCentresTheImage()
{
    // A portrait image on a portrait page fills the height, so the margin is all
    // horizontal and the vertical offset is nothing.
    const QPointF origin = originFor(QSize(1000, 2000), page());
    QVERIFY(origin.x() > 0.0);
    QVERIFY(qAbs(origin.y()) < 1e-9);
}

void TestPdfExport::originIsZeroForAnExactFit()
{
    const QPointF origin = originFor(QSize(int(PageWidth), int(PageHeight)), page());
    QVERIFY(qAbs(origin.x()) < 1.0);
    QVERIFY(qAbs(origin.y()) < 1.0);
}

void TestPdfExport::writesAFile()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.path() + QStringLiteral("/out.pdf");

    QVERIFY(write(path, photo(600, 800), QStringLiteral("Comites de quartier")));

    QFile file(path);
    QVERIFY(file.exists());
    QVERIFY(file.size() > 0);

    // It is a PDF, by its own first bytes rather than by its name.
    QVERIFY(file.open(QIODevice::ReadOnly));
    QVERIFY(file.read(5).startsWith("%PDF-"));
}

void TestPdfExport::writesWithoutAnyText()
{
    // A photograph that gave up nothing still exports: the picture is the point,
    // and refusing would lose it over the part that failed.
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.path() + QStringLiteral("/empty.pdf");

    QVERIFY(write(path, photo(400, 300), QString()));
    QVERIFY(QFile(path).size() > 0);

    QVERIFY(write(path, photo(400, 300), QStringLiteral("   \n  \n ")));
    QVERIFY(QFile(path).size() > 0);
}

void TestPdfExport::refusesAnEmptyPhoto()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QVERIFY(!write(dir.path() + QStringLiteral("/none.pdf"), QImage(),
                   QStringLiteral("text")));
}

void TestPdfExport::longTextRunsToSeveralPages()
{
    // Enough text to need more than one page after the photograph. The failure
    // this guards against is a layout that silently drops everything past the
    // first page, which looks like a complete file until somebody reads to the
    // bottom of it.
    QString wall;
    for (int i = 0; i < 400; ++i) {
        wall += QStringLiteral("Comites de quartier, Conseil des sages, reunions "
                               "publiques, visites de quartier. ");
    }

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString shortPath = dir.path() + QStringLiteral("/short.pdf");
    const QString longPath = dir.path() + QStringLiteral("/long.pdf");

    QVERIFY(write(shortPath, photo(600, 800), QStringLiteral("one line")));
    QVERIFY(write(longPath, photo(600, 800), wall));

    // Not a page count - that would mean parsing a PDF - but the file with forty
    // times the text must be substantially larger, which it cannot be if the
    // overflow was dropped.
    QVERIFY2(QFile(longPath).size() > QFile(shortPath).size() * 2,
             qPrintable(QStringLiteral("short %1, long %2")
                            .arg(QFile(shortPath).size())
                            .arg(QFile(longPath).size())));
}

QTEST_MAIN(TestPdfExport)
#include "tst_pdfexport.moc"
