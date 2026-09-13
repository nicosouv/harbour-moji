#include <QtTest>

#include "tableextract.h"

using namespace TableExtract;

// Reading a table out of a page that never said it was one.
//
// The fixtures are grids whose right answer is known, plus the cases that make
// the naive version wrong: a paragraph must not be mistaken for a table, a word
// that overhangs a boundary must land in the column it sits in, and a cell that
// contains a comma must survive being written as CSV.
class TestTableExtract : public QObject
{
    Q_OBJECT

private:
    // Words are 20 tall; columns start at x = 0, 200 and 400, each about 90 wide,
    // so the channels between them are far wider than a word space.
    static OcrResult grid()
    {
        OcrResult result;
        result.setImageSize(QSize(600, 200));

        const char *text[3][3] = {
            { "Item",   "Qty", "Price" },
            { "Coffee", "2",   "4.50"  },
            { "Tea",    "1",   "3.00"  },
        };

        for (int row = 0; row < 3; ++row) {
            for (int column = 0; column < 3; ++column) {
                OcrWord word;
                word.text = QString::fromLatin1(text[row][column]);
                word.box = QRect(column * 200, row * 40, 90, 20);
                word.confidence = 90.0f;
                word.line = row;
                word.paragraph = 0;
                word.block = 0;
                result.append(word);
            }
        }
        return result;
    }

    // One line of running text: words close together, no vertical channel.
    static OcrResult paragraph()
    {
        OcrResult result;
        result.setImageSize(QSize(600, 200));

        const QStringList words = { "the", "quick", "brown", "fox", "jumps", "over" };
        int x = 0;
        for (const QString &text : words) {
            OcrWord word;
            word.text = text;
            word.box = QRect(x, 0, 50, 20);
            word.confidence = 90.0f;
            word.line = 0;
            word.paragraph = 0;
            word.block = 0;
            result.append(word);
            x += 58;   // an ordinary word space, well under a word's height
        }
        return result;
    }

private slots:
    void findsTheColumnBoundaries();
    void countsColumns();
    void refusesAParagraph();
    void refusesTooLittleText();
    void buildsTheCells();
    void padsAShortRow();
    void placesAWordByItsCentre();
    void escapesACommaInACell();
    void escapesAQuoteByDoubling();
    void leavesAnOrdinaryFieldAlone();
    void writesTheWholeTable();
};

void TestTableExtract::findsTheColumnBoundaries()
{
    // Two boundaries for three columns, each in the middle of its channel.
    const QVector<int> edges = columnEdges(grid(), 0);
    QCOMPARE(edges.size(), 2);
    QVERIFY(edges.at(0) > 89 && edges.at(0) < 200);
    QVERIFY(edges.at(1) > 289 && edges.at(1) < 400);
}

void TestTableExtract::countsColumns()
{
    QCOMPARE(columnCount(grid(), 0), 3);
}

void TestTableExtract::refusesAParagraph()
{
    // Running text has no channel wider than a word space. Offering a CSV export
    // for a paragraph would be worse than not offering one at all.
    QCOMPARE(columnCount(paragraph(), 0), 0);
    QVERIFY(toCsv(paragraph(), 0).isEmpty());
}

void TestTableExtract::refusesTooLittleText()
{
    OcrResult sparse;
    OcrWord word;
    word.text = QStringLiteral("alone");
    word.box = QRect(0, 0, 50, 20);
    sparse.append(word);

    QVERIFY(columnEdges(sparse, 0).isEmpty());
}

void TestTableExtract::buildsTheCells()
{
    const QVector<QVector<QString>> rows = cells(grid(), 0);
    QCOMPARE(rows.size(), 3);
    QCOMPARE(rows.at(0), QVector<QString>({ "Item", "Qty", "Price" }));
    QCOMPARE(rows.at(1), QVector<QString>({ "Coffee", "2", "4.50" }));
    QCOMPARE(rows.at(2), QVector<QString>({ "Tea", "1", "3.00" }));
}

void TestTableExtract::padsAShortRow()
{
    OcrResult result = grid();
    // A row that stops after two columns, as a subtotal line does.
    OcrWord word;
    word.text = QStringLiteral("Total");
    word.box = QRect(0, 120, 90, 20);
    word.confidence = 90.0f;
    word.line = 3;
    word.block = 0;
    result.append(word);

    const QVector<QVector<QString>> rows = cells(result, 0);
    QCOMPARE(rows.size(), 4);
    // Every row the same width, or it is not a CSV.
    QCOMPARE(rows.at(3).size(), 3);
    QCOMPARE(rows.at(3).at(0), QStringLiteral("Total"));
    QVERIFY(rows.at(3).at(1).isEmpty());
}

void TestTableExtract::placesAWordByItsCentre()
{
    OcrResult result = grid();
    // A long word starting in column 0 and overhanging the first boundary. Its
    // centre is still in column 0, so that is where it belongs; placing by the
    // left edge would agree here, but placing by the right edge would not.
    OcrWord word;
    word.text = QStringLiteral("Cappuccino");
    word.box = QRect(0, 160, 150, 20);
    word.confidence = 90.0f;
    word.line = 4;
    word.block = 0;
    result.append(word);

    const QVector<QVector<QString>> rows = cells(result, 0);
    QCOMPARE(rows.last().at(0), QStringLiteral("Cappuccino"));
}

void TestTableExtract::escapesACommaInACell()
{
    // A European price. Unescaped it would split one column into two, and the
    // damage would only show up in a spreadsheet much later.
    QCOMPARE(escapeField(QStringLiteral("1,50")), QStringLiteral("\"1,50\""));
}

void TestTableExtract::escapesAQuoteByDoubling()
{
    QCOMPARE(escapeField(QStringLiteral("say \"hi\"")),
             QStringLiteral("\"say \"\"hi\"\"\""));
}

void TestTableExtract::leavesAnOrdinaryFieldAlone()
{
    QCOMPARE(escapeField(QStringLiteral("Coffee")), QStringLiteral("Coffee"));
}

void TestTableExtract::writesTheWholeTable()
{
    QCOMPARE(toCsv(grid(), 0),
             QStringLiteral("Item,Qty,Price\nCoffee,2,4.50\nTea,1,3.00"));
}

QTEST_APPLESS_MAIN(TestTableExtract)
#include "tst_tableextract.moc"
