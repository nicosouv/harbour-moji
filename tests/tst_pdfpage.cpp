#include <QtTest>
#include <QTemporaryFile>

#include "pdfpage.h"

using namespace PdfPage;

// Choosing the resolution to rasterise a PDF page at.
//
// A PDF page has no pixels, only a size in points, so this number is invented -
// and both ways of getting it wrong are quiet. Too low and the recogniser sees
// text four pixels tall and reports nothing, which looks like a bad PDF. Too high
// and an A4 page becomes 140MB of ARGB32 and the process is killed, which looks
// like a crash. Neither says "the DPI was wrong".
class TestPdfPage : public QObject
{
    Q_OBJECT

private slots:
    void smallPageGetsTheTrainedResolution();
    void a4ComesDownToTheBudget();
    void resultNeverExceedsTheBudget_data();
    void resultNeverExceedsTheBudget();
    void anAbsurdPageStillFitsTheBudget();
    void degeneratePageIsNotADivideByZero();

    void looksLikePdfReadsTheBytes();
    void looksLikePdfRejectsAnImposter();
    void looksLikePdfRejectsAMissingFile();
};

void TestPdfPage::smallPageGetsTheTrainedResolution()
{
    // A receipt: 200x400 points is 1667 pixels at 300 DPI, well inside the
    // budget, so there is no reason to render it at anything else.
    QCOMPARE(resolutionFor(QSizeF(200, 400), 2400), PreferredDpi);
}

void TestPdfPage::a4ComesDownToTheBudget()
{
    // A4 is 595x842 points. At 300 DPI that is 3508 pixels on the long edge,
    // which ImagePrep would immediately scale back to 2400 - so the extra pixels
    // are pure cost. 2400 * 72 / 842 = 205.2.
    const qreal dpi = resolutionFor(QSizeF(595.3, 841.9), 2400);
    QVERIFY(dpi < PreferredDpi);
    QVERIFY(qAbs(dpi - 205.2) < 0.5);
}

void TestPdfPage::resultNeverExceedsTheBudget_data()
{
    QTest::addColumn<QSizeF>("size");
    QTest::newRow("A4 portrait") << QSizeF(595.3, 841.9);
    QTest::newRow("A4 landscape") << QSizeF(841.9, 595.3);
    QTest::newRow("A3") << QSizeF(841.9, 1190.6);
    QTest::newRow("US Letter") << QSizeF(612, 792);
    QTest::newRow("receipt") << QSizeF(164, 900);
    QTest::newRow("business card") << QSizeF(252, 144);
    QTest::newRow("A0 poster") << QSizeF(2384, 3370);
}

void TestPdfPage::resultNeverExceedsTheBudget()
{
    QFETCH(QSizeF, size);

    const int maxEdge = 2400;
    const qreal dpi = resolutionFor(size, maxEdge);

    // The property that matters: whatever page it is handed, the render it asks
    // for fits in memory. A page one pixel over is not a problem; a page four
    // times over is the one that gets the app killed.
    const qreal longest = qMax(size.width(), size.height());
    const qreal pixels = longest * dpi / 72.0;
    QVERIFY2(pixels <= maxEdge + 1.0,
             qPrintable(QStringLiteral("%1 px at %2 dpi").arg(pixels).arg(dpi)));
}

void TestPdfPage::anAbsurdPageStillFitsTheBudget()
{
    // A plan, or a poster. There was a floor of 72 DPI here first, so that a page
    // this size would not be rendered unreadably small - and it was wrong: at 72
    // DPI this page is 30000 pixels on its long edge, twelve times the budget and
    // several gigabytes as ARGB32. The budget wins. Rendering it at 5 DPI reads
    // nothing, and reading nothing is a result; being killed is not.
    const qreal dpi = resolutionFor(QSizeF(20000, 30000), 2400);
    QVERIFY(30000.0 * dpi / 72.0 <= 2401.0);
}

void TestPdfPage::degeneratePageIsNotADivideByZero()
{
    QCOMPARE(resolutionFor(QSizeF(0, 0), 2400), PreferredDpi);
    QCOMPARE(resolutionFor(QSizeF(595, 842), 0), PreferredDpi);
}

void TestPdfPage::looksLikePdfReadsTheBytes()
{
    QTemporaryFile file;
    QVERIFY(file.open());
    file.write("%PDF-1.7\n1 0 obj\n");
    file.flush();
    QVERIFY(looksLikePdf(file.fileName()));
}

void TestPdfPage::looksLikePdfRejectsAnImposter()
{
    // A JPEG someone renamed. Trusting the extension means Poppler is asked to
    // open it and the user is told their PDF is damaged, about a file that was
    // never a PDF - an error that sends them looking in the wrong place.
    QTemporaryFile file;
    QVERIFY(file.open());
    file.write(QByteArray::fromHex("ffd8ffe000104a46494600010100000100010000"));
    file.flush();
    QVERIFY(!looksLikePdf(file.fileName()));
}

void TestPdfPage::looksLikePdfRejectsAMissingFile()
{
    QVERIFY(!looksLikePdf(QStringLiteral("/nowhere/at/all.pdf")));
}

QTEST_APPLESS_MAIN(TestPdfPage)
#include "tst_pdfpage.moc"
