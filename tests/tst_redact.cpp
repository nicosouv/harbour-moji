#include <QtTest>

#include "fieldparser.h"
#include "ocrresult.h"

// Finding, in the photo, the words a field came from.
//
// fieldparser works on the joined text and reports an offset into that string.
// The join inserts spaces and newlines that exist in the string and nowhere in
// the image, so mapping an offset back to word boxes has to replay the join. Get
// it wrong by one separator and the black box lands on the word next door - which
// is the worst possible failure for a redaction, because it looks like it worked.
class TestRedact : public QObject
{
    Q_OBJECT

private:
    static OcrResult page()
    {
        OcrResult result;
        result.setImageSize(QSize(800, 400));

        struct Entry { const char *text; int line; int paragraph; };
        const Entry entries[] = {
            { "Invoice", 0, 0 },
            { "total",   0, 0 },
            { "IBAN",    1, 0 },
            { "GB82",    1, 0 },
            { "WEST",    1, 0 },
            { "1234",    1, 0 },
            { "5698",    1, 0 },
            { "7654",    1, 0 },
            { "32",      1, 0 },
            { "Thanks",  2, 1 },   // a new paragraph: two separator characters
        };

        int x = 10;
        for (const Entry &entry : entries) {
            OcrWord word;
            word.text = QString::fromLatin1(entry.text);
            word.box = QRect(x, entry.line * 40, 60, 20);
            word.confidence = 90.0f;
            word.line = entry.line;
            word.paragraph = entry.paragraph;
            word.block = 0;
            result.append(word);
            x += 70;
        }
        return result;
    }

private slots:
    void firstWordStartsAtZero();
    void mapsAWordAfterANewline();
    void mapsAWordAfterAParagraphBreak();
    void mapsTheWholeIban();
    void rejectsAnEmptyRange();
    void handlesARangePastTheEnd();
};

void TestRedact::firstWordStartsAtZero()
{
    const OcrResult result = page();
    QCOMPARE(result.wordsForRange(0, 7), QVector<int>({ 0 }));
}

void TestRedact::mapsAWordAfterANewline()
{
    const OcrResult result = page();
    const QString text = result.text();

    // "IBAN" sits after "Invoice total\n" - one separator per gap, and the line
    // break counts as one character, not two.
    const int at = text.indexOf(QStringLiteral("IBAN"));
    QVERIFY(at > 0);
    QCOMPARE(result.wordsForRange(at, 4), QVector<int>({ 2 }));
}

void TestRedact::mapsAWordAfterAParagraphBreak()
{
    const OcrResult result = page();
    const QString text = result.text();

    // "Thanks" is in a new paragraph, so two characters separate it. An
    // off-by-one here would return the word before it.
    const int at = text.indexOf(QStringLiteral("Thanks"));
    QVERIFY(at > 0);
    QCOMPARE(result.wordsForRange(at, 6), QVector<int>({ 9 }));
}

void TestRedact::mapsTheWholeIban()
{
    const OcrResult result = page();

    // What redaction actually asks: fieldparser found an IBAN, which words is it?
    const QVector<FieldParser::Field> fields = FieldParser::scan(result.text());

    bool checked = false;
    for (const FieldParser::Field &field : fields) {
        if (field.kind != FieldParser::Iban) {
            continue;
        }
        const QVector<int> words = result.wordsForRange(field.start, field.length);

        // The six words of the number, and not the "IBAN" label before it nor
        // "Thanks" after.
        QCOMPARE(words, QVector<int>({ 3, 4, 5, 6, 7, 8 }));
        checked = true;
    }
    QVERIFY2(checked, "the fixture should contain a detectable IBAN");
}

void TestRedact::rejectsAnEmptyRange()
{
    QVERIFY(page().wordsForRange(0, 0).isEmpty());
    QVERIFY(page().wordsForRange(5, -1).isEmpty());
}

void TestRedact::handlesARangePastTheEnd()
{
    const OcrResult result = page();
    // Must not read past the list; the last word is a legitimate answer.
    const QVector<int> words = result.wordsForRange(result.text().length() - 3, 999);
    QCOMPARE(words, QVector<int>({ 9 }));
}

QTEST_APPLESS_MAIN(TestRedact)
#include "tst_redact.moc"
