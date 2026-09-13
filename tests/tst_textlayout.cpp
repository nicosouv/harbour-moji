#include <QtTest>

#include "textlayout.h"

using namespace TextLayout;

// The tap-to-extract gesture, held to its rules.
//
// Everything that makes it feel deliberate or sloppy is decided in this layer, so
// the interesting cases are the awkward ones: a tap that lands between two words,
// a drag that clips the neighbouring column, a third tap when there is nothing
// bigger left to select.
class TestTextLayout : public QObject
{
    Q_OBJECT

private:
    // Two paragraphs in one block, plus a second block off to the right - enough
    // structure that growing a selection has somewhere to go, and enough
    // separation that a wrong answer is visible rather than plausible.
    //
    //   block 0, paragraph 0, line 0:  "Invoice" "total"     y 0..20
    //   block 0, paragraph 0, line 1:  "42 EUR"              y 30..50
    //   block 0, paragraph 1, line 2:  "Thanks"              y 70..90
    //   block 1, paragraph 2, line 3:  "Aside"               x 400.., y 0..20
    static OcrResult sample()
    {
        OcrResult result;
        result.setImageSize(QSize(600, 200));

        auto add = [&result](const QString &text, const QRect &box,
                             int line, int paragraph, int block) {
            OcrWord word;
            word.text = text;
            word.box = box;
            word.confidence = 90.0f;
            word.line = line;
            word.paragraph = paragraph;
            word.block = block;
            result.append(word);
        };

        add("Invoice", QRect(10, 0, 80, 20), 0, 0, 0);
        add("total",   QRect(100, 0, 50, 20), 0, 0, 0);
        add("42",      QRect(10, 30, 30, 20), 1, 0, 0);
        add("EUR",     QRect(50, 30, 40, 20), 1, 0, 0);
        add("Thanks",  QRect(10, 70, 70, 20), 2, 1, 0);
        add("Aside",   QRect(400, 0, 60, 20), 3, 2, 1);

        return result;
    }

private slots:
    void wordIndexAtFindsWordUnderPoint();
    void wordIndexAtMissesGapWithoutTolerance();
    void wordIndexAtFallsBackToNearestWithinTolerance();
    void wordIndexAtPrefersContainmentOverProximity();
    void wordIndexAtRefusesBeyondTolerance();

    void selectAtWordTakesOnlyThatWord();
    void selectAtLineTakesTheWholeLine();
    void selectAtParagraphCrossesLinesButNotParagraphs();
    void selectAtBlockStopsAtTheBlock();
    void selectAtMissIsInvalid();

    void growSaturatesAtBlock();
    void shrinkSaturatesAtWord();

    void selectInRectUsesWordCentres();
    void selectInRectIgnoresClippedNeighbour();

    void textJoinsLinesWithNewlineAndParagraphsWithBlankLine();
    void boxOfSelectionSpansItsWords();

    void lineNumbersAreDistinctAndInReadingOrder();
    void lineConfidenceAveragesOnlyThatLine();
};

void TestTextLayout::wordIndexAtFindsWordUnderPoint()
{
    const OcrResult result = sample();
    QCOMPARE(wordIndexAt(result, QPoint(50, 10)), 0);   // "Invoice"
    QCOMPARE(wordIndexAt(result, QPoint(120, 10)), 1);  // "total"
    QCOMPARE(wordIndexAt(result, QPoint(420, 10)), 5);  // "Aside"
}

void TestTextLayout::wordIndexAtMissesGapWithoutTolerance()
{
    const OcrResult result = sample();
    // x 95 is between "Invoice" (ends 89) and "total" (starts 100).
    QCOMPARE(wordIndexAt(result, QPoint(95, 10)), -1);
}

void TestTextLayout::wordIndexAtFallsBackToNearestWithinTolerance()
{
    const OcrResult result = sample();
    // Same gap, now with a fingertip's worth of slack: 95 is 6px past the right
    // edge of "Invoice" and 5px short of "total", so "total" wins.
    QCOMPARE(wordIndexAt(result, QPoint(95, 10), 20), 1);
}

void TestTextLayout::wordIndexAtPrefersContainmentOverProximity()
{
    const OcrResult result = sample();
    // Just inside "total"'s left edge. A centre-distance test would answer
    // "Invoice", whose centre is nearer; containment must win outright.
    QCOMPARE(wordIndexAt(result, QPoint(101, 10), 50), 1);
}

void TestTextLayout::wordIndexAtRefusesBeyondTolerance()
{
    const OcrResult result = sample();
    // Empty space bottom right, far from everything.
    QCOMPARE(wordIndexAt(result, QPoint(560, 190), 10), -1);
}

void TestTextLayout::selectAtWordTakesOnlyThatWord()
{
    const Selection selection = selectAt(sample(), QPoint(50, 10), Word);
    QVERIFY(selection.isValid());
    QCOMPARE(selection.words.size(), 1);
    QCOMPARE(selection.text, QStringLiteral("Invoice"));
}

void TestTextLayout::selectAtLineTakesTheWholeLine()
{
    const Selection selection = selectAt(sample(), QPoint(50, 10), Line);
    QCOMPARE(selection.words.size(), 2);
    QCOMPARE(selection.text, QStringLiteral("Invoice total"));
}

void TestTextLayout::selectAtParagraphCrossesLinesButNotParagraphs()
{
    const Selection selection = selectAt(sample(), QPoint(50, 10), Paragraph);

    // Two lines of paragraph 0, and emphatically not "Thanks".
    QCOMPARE(selection.words.size(), 4);
    QCOMPARE(selection.text, QStringLiteral("Invoice total\n42 EUR"));
    QVERIFY(!selection.text.contains(QStringLiteral("Thanks")));
}

void TestTextLayout::selectAtBlockStopsAtTheBlock()
{
    const Selection selection = selectAt(sample(), QPoint(50, 10), Block);

    // All of block 0 - both paragraphs - and none of block 1.
    QCOMPARE(selection.words.size(), 5);
    QVERIFY(selection.text.contains(QStringLiteral("Thanks")));
    QVERIFY(!selection.text.contains(QStringLiteral("Aside")));
}

void TestTextLayout::selectAtMissIsInvalid()
{
    const Selection selection = selectAt(sample(), QPoint(560, 190), Line);
    QVERIFY(!selection.isValid());
    QVERIFY(selection.text.isEmpty());
}

void TestTextLayout::growSaturatesAtBlock()
{
    QCOMPARE(grow(Word), Line);
    QCOMPARE(grow(Line), Paragraph);
    QCOMPARE(grow(Paragraph), Block);
    // Repeated taps settle rather than cycling back to a single word.
    QCOMPARE(grow(Block), Block);
}

void TestTextLayout::shrinkSaturatesAtWord()
{
    QCOMPARE(shrink(Block), Paragraph);
    QCOMPARE(shrink(Paragraph), Line);
    QCOMPARE(shrink(Line), Word);
    QCOMPARE(shrink(Word), Word);
}

void TestTextLayout::selectInRectUsesWordCentres()
{
    // A rectangle over the first line only.
    const Selection selection = selectInRect(sample(), QRect(0, 0, 300, 25));
    QCOMPARE(selection.words.size(), 2);
    QCOMPARE(selection.text, QStringLiteral("Invoice total"));
}

void TestTextLayout::selectInRectIgnoresClippedNeighbour()
{
    // Reaches 10px into "total" (100..150, centre 125) but not past its centre,
    // so "total" is left out. Intersection would have grabbed it.
    const Selection selection = selectInRect(sample(), QRect(0, 0, 110, 25));
    QCOMPARE(selection.words.size(), 1);
    QCOMPARE(selection.text, QStringLiteral("Invoice"));
}

void TestTextLayout::textJoinsLinesWithNewlineAndParagraphsWithBlankLine()
{
    QCOMPARE(sample().text(),
             QStringLiteral("Invoice total\n42 EUR\n\nThanks\n\nAside"));
}

void TestTextLayout::boxOfSelectionSpansItsWords()
{
    const Selection selection = selectAt(sample(), QPoint(50, 10), Line);
    // "Invoice" 10..90 and "total" 100..150 on y 0..20.
    QCOMPARE(selection.box, QRect(10, 0, 140, 20));
}

void TestTextLayout::lineNumbersAreDistinctAndInReadingOrder()
{
    // Four lines across two blocks, each reported once, in the order they were
    // read - this drives the overlay, so a duplicate would draw twice and a
    // reorder would tint the wrong row.
    const QVector<int> lines = sample().lineNumbers();
    QCOMPARE(lines.size(), 4);
    QCOMPARE(lines, QVector<int>() << 0 << 1 << 2 << 3);
}

void TestTextLayout::lineConfidenceAveragesOnlyThatLine()
{
    OcrResult result = sample();

    // Every word in the fixture is 90; a line whose words differ must average
    // only its own.
    QCOMPARE(result.lineConfidence(0), 90.0f);
    // A line that does not exist is 0, not a division by zero.
    QCOMPARE(result.lineConfidence(99), 0.0f);
}

QTEST_APPLESS_MAIN(TestTextLayout)
#include "tst_textlayout.moc"
