#include <QtTest>

#include "fieldparser.h"
#include "ocrresult.h"
#include "textlayout.h"

// Correcting a word the recogniser got wrong.
//
// The point of this test is the chain, not the setter. A correction is only worth
// having if it reaches everything derived from the text - and the case that makes
// it obviously worth having is a misread IBAN: fix the digit the camera got wrong
// and the checksum has to go from "does not match" to "verified", because that is
// the difference between a number you can use and one you cannot.
class TestCorrection : public QObject
{
    Q_OBJECT

private:
    // "GB82 WEST 1234 5698 7654 32" is the registry's own example and passes
    // mod-97. Here the recogniser read the 8 as a B, which is exactly the
    // substitution OCR makes and exactly what a checksum is for.
    static OcrResult misreadIban()
    {
        OcrResult result;
        result.setImageSize(QSize(600, 200));

        struct Entry { const char *text; float confidence; };
        const Entry entries[] = {
            { "IBAN", 96.0f },
            { "GB82", 95.0f },
            { "WEST", 94.0f },
            { "1234", 93.0f },
            { "569B", 41.0f },   // should be 5698 - and the engine knows it is unsure
            { "7654", 92.0f },
            { "32", 91.0f },
        };

        int x = 10;
        for (const Entry &entry : entries) {
            OcrWord word;
            word.text = QString::fromLatin1(entry.text);
            word.box = QRect(x, 0, 40, 20);
            word.confidence = entry.confidence;
            word.line = 0;
            word.paragraph = 0;
            word.block = 0;
            result.append(word);
            x += 50;
        }
        return result;
    }

private slots:
    void uncertainWordsAreTheDoubtfulOnes();
    void correctionChangesTheText();
    void correctionMarksTheWordCertain();
    void correctedWordStopsBeingOffered();
    void correctionRescuesTheChecksum();
    void correctionReachesASelection();
    void correctionIgnoresAnIndexOutOfRange();
};

void TestCorrection::uncertainWordsAreTheDoubtfulOnes()
{
    const QVector<int> uncertain = misreadIban().uncertainWords();
    QCOMPARE(uncertain.size(), 1);
    QCOMPARE(uncertain.first(), 4);
}

void TestCorrection::correctionChangesTheText()
{
    OcrResult result = misreadIban();
    QVERIFY(result.text().contains(QStringLiteral("569B")));

    result.setWordText(4, QStringLiteral("5698"));

    QVERIFY(result.text().contains(QStringLiteral("5698")));
    QVERIFY(!result.text().contains(QStringLiteral("569B")));
}

void TestCorrection::correctionMarksTheWordCertain()
{
    OcrResult result = misreadIban();
    result.setWordText(4, QStringLiteral("5698"));

    QVERIFY(result.words().at(4).corrected);
    QCOMPARE(result.words().at(4).confidence, 100.0f);
}

void TestCorrection::correctedWordStopsBeingOffered()
{
    OcrResult result = misreadIban();
    result.setWordText(4, QStringLiteral("5698"));

    // Still highlighted after being retyped would be the app arguing with the
    // user about a word they just told it.
    QVERIFY(result.uncertainWords().isEmpty());
}

void TestCorrection::correctionRescuesTheChecksum()
{
    OcrResult result = misreadIban();

    // Before: found, shaped like an IBAN, and mod-97 disagrees.
    QVector<FieldParser::Field> before = FieldParser::scan(result.text());
    bool sawBroken = false;
    for (const FieldParser::Field &field : before) {
        if (field.kind == FieldParser::Iban) {
            sawBroken = !field.checksumValid;
        }
    }
    QVERIFY2(sawBroken, "the misread IBAN should be reported and fail its checksum");

    result.setWordText(4, QStringLiteral("5698"));

    // After: the same number, now verified. This is the whole feature.
    QVector<FieldParser::Field> after = FieldParser::scan(result.text());
    bool sawValid = false;
    for (const FieldParser::Field &field : after) {
        if (field.kind == FieldParser::Iban && field.checksumValid) {
            sawValid = true;
            QCOMPARE(field.normalised, QStringLiteral("GB82WEST12345698765432"));
        }
    }
    QVERIFY2(sawValid, "correcting the digit should make mod-97 agree");
}

void TestCorrection::correctionReachesASelection()
{
    OcrResult result = misreadIban();
    result.setWordText(4, QStringLiteral("5698"));

    // A selection is computed from the result, not cached, so it carries the
    // correction without anything having to invalidate it.
    const TextLayout::Selection line =
        TextLayout::selectAt(result, QPoint(220, 10), TextLayout::Line);
    QVERIFY(line.isValid());
    QVERIFY(line.text.contains(QStringLiteral("5698")));
}

void TestCorrection::correctionIgnoresAnIndexOutOfRange()
{
    OcrResult result = misreadIban();
    const QString before = result.text();

    result.setWordText(-1, QStringLiteral("x"));
    result.setWordText(999, QStringLiteral("x"));

    QCOMPARE(result.text(), before);
}

QTEST_APPLESS_MAIN(TestCorrection)
#include "tst_correction.moc"
