#include <QtTest>

#include "fieldparser.h"

using namespace FieldParser;

// The checksums are the reason this layer exists, so they are held to published
// reference values rather than to values this code produced. A checksum that is
// merely self-consistent is worse than none: it would confidently pass numbers a
// camera got wrong, which is the exact failure it was added to prevent.
class TestFieldParser : public QObject
{
    Q_OBJECT

private slots:
    void ibanAcceptsPublishedExamples_data();
    void ibanAcceptsPublishedExamples();
    void ibanRejectsAlteredDigit();
    void ibanRejectsWrongLength();

    void luhnAcceptsKnownTestCards_data();
    void luhnAcceptsKnownTestCards();
    void luhnRejectsAlteredDigit();

    void isbn10Accepts();
    void isbn10AcceptsTrailingX();
    void isbn13Accepts();
    void isbnRejectsAlteredDigit();
    void isbnRejectsWrongLength();

    void mrzCheckDigitMatchesIcaoExample();
    void mrzTd3AcceptsIcaoExample();
    void mrzTd3RejectsAlteredDigit();

    void scanFindsIbanInInvoiceText();
    void scanReportsIbanWithBadChecksum();
    void scanDoesNotCallADigitRunACard();
    void scanReturnsFieldsInReadingOrder();
    void scanLocatesFieldByOffset();
};

// ---- IBAN ----------------------------------------------------------------

void TestFieldParser::ibanAcceptsPublishedExamples_data()
{
    QTest::addColumn<QString>("iban");

    // The registry's own examples, spacing as a human would write it.
    QTest::newRow("GB") << "GB82 WEST 1234 5698 7654 32";
    QTest::newRow("DE") << "DE89 3704 0044 0532 0130 00";
    QTest::newRow("FR") << "FR14 2004 1010 0505 0001 3M02 606";
    QTest::newRow("no spaces") << "GB82WEST12345698765432";
}

void TestFieldParser::ibanAcceptsPublishedExamples()
{
    QFETCH(QString, iban);
    QVERIFY(ibanValid(iban));
}

void TestFieldParser::ibanRejectsAlteredDigit()
{
    // Last digit moved by one: precisely the single-character substitution mod-97
    // is there to catch.
    QVERIFY(!ibanValid("GB82 WEST 1234 5698 7654 33"));
}

void TestFieldParser::ibanRejectsWrongLength()
{
    QVERIFY(!ibanValid("GB82 WEST"));
    QVERIFY(!ibanValid(""));
}

// ---- Luhn ---------------------------------------------------------------

void TestFieldParser::luhnAcceptsKnownTestCards_data()
{
    QTest::addColumn<QString>("number");

    QTest::newRow("visa test") << "4242424242424242";
    QTest::newRow("visa test 2") << "4111111111111111";
    QTest::newRow("mastercard test") << "5555555555554444";
    QTest::newRow("amex test, 15 digits") << "378282246310005";
    QTest::newRow("spaced") << "4242 4242 4242 4242";
}

void TestFieldParser::luhnAcceptsKnownTestCards()
{
    QFETCH(QString, number);
    QVERIFY(luhnValid(number));
}

void TestFieldParser::luhnRejectsAlteredDigit()
{
    QVERIFY(!luhnValid("4242424242424241"));
    QVERIFY(!luhnValid("4111111111111121"));
}

// ---- ISBN ---------------------------------------------------------------

void TestFieldParser::isbn10Accepts()
{
    QVERIFY(isbnValid("0-306-40615-2"));
    QVERIFY(isbnValid("0306406152"));
}

void TestFieldParser::isbn10AcceptsTrailingX()
{
    // X in the check position means 10, and only there.
    QVERIFY(isbnValid("0-8044-2957-X"));
    QVERIFY(!isbnValid("0-8044-295X-7"));
}

void TestFieldParser::isbn13Accepts()
{
    QVERIFY(isbnValid("978-0-306-40615-7"));
    QVERIFY(isbnValid("9780306406157"));
}

void TestFieldParser::isbnRejectsAlteredDigit()
{
    QVERIFY(!isbnValid("0-306-40615-3"));
    QVERIFY(!isbnValid("978-0-306-40615-8"));
}

void TestFieldParser::isbnRejectsWrongLength()
{
    QVERIFY(!isbnValid("03064061"));
    QVERIFY(!isbnValid("97803064061570"));
}

// ---- MRZ ----------------------------------------------------------------

namespace {

// ICAO Doc 9303's specimen passport for Utopia.
const char *kMrzLine1 = "P<UTOERIKSSON<<ANNA<MARIA<<<<<<<<<<<<<<<<<<<";
const char *kMrzLine2 = "L898902C36UTO7408122F1204159ZE184226B<<<<<10";

} // namespace

void TestFieldParser::mrzCheckDigitMatchesIcaoExample()
{
    // Both lines must be exactly 44 or every offset below is meaningless.
    QCOMPARE(QString(kMrzLine1).length(), 44);
    QCOMPARE(QString(kMrzLine2).length(), 44);

    // The document number "L898902C3" carries check digit 6.
    QCOMPARE(mrzCheckDigit("L898902C3"), 6);
    // Date of birth 740812 carries 2.
    QCOMPARE(mrzCheckDigit("740812"), 2);
    // Expiry 120415 carries 9.
    QCOMPARE(mrzCheckDigit("120415"), 9);
}

void TestFieldParser::mrzTd3AcceptsIcaoExample()
{
    QVERIFY(mrzTd3Valid(QString(kMrzLine1), QString(kMrzLine2)));
}

void TestFieldParser::mrzTd3RejectsAlteredDigit()
{
    QString line2(kMrzLine2);
    line2[3] = QLatin1Char('1');  // inside the document number
    QVERIFY(!mrzTd3Valid(QString(kMrzLine1), line2));

    // Wrong length must not be read as a near miss.
    QVERIFY(!mrzTd3Valid("P<UTO", QString(kMrzLine2)));
}

// ---- scan ---------------------------------------------------------------

void TestFieldParser::scanFindsIbanInInvoiceText()
{
    const QString text = QStringLiteral(
        "Please transfer to\nGB82 WEST 1234 5698 7654 32\nreference 4471");

    const QVector<Field> fields = scan(text);

    int ibans = 0;
    for (const Field &field : fields) {
        if (field.kind == Iban) {
            ++ibans;
            QVERIFY(field.checksumValid);
            QCOMPARE(field.normalised, QStringLiteral("GB82WEST12345698765432"));
        }
    }
    QCOMPARE(ibans, 1);
}

void TestFieldParser::scanReportsIbanWithBadChecksum()
{
    // An IBAN shape with wrong digits is still reported, flagged invalid: telling
    // the user which line to re-read is more useful than silence.
    const QVector<Field> fields = scan(QStringLiteral("IBAN GB82 WEST 1234 5698 7654 33"));

    bool sawInvalidIban = false;
    for (const Field &field : fields) {
        if (field.kind == Iban && !field.checksumValid) {
            sawInvalidIban = true;
        }
    }
    QVERIFY(sawInvalidIban);
}

void TestFieldParser::scanDoesNotCallADigitRunACard()
{
    // 16 digits that fail Luhn are not a card number, and must not be offered as
    // one - labelling a random reference number "Card number" is worse than
    // missing a real card.
    const QVector<Field> fields = scan(QStringLiteral("Order 1234567890123456 shipped"));

    for (const Field &field : fields) {
        QVERIFY(field.kind != CreditCard);
    }
}

void TestFieldParser::scanReturnsFieldsInReadingOrder()
{
    const QString text = QStringLiteral(
        "mail me at bob@example.com\nor pay GB82 WEST 1234 5698 7654 32");

    const QVector<Field> fields = scan(text);
    QVERIFY(fields.size() >= 2);

    for (int i = 1; i < fields.size(); ++i) {
        QVERIFY(fields.at(i - 1).start <= fields.at(i).start);
    }
}

void TestFieldParser::scanLocatesFieldByOffset()
{
    const QString text = QStringLiteral("write to bob@example.com now");
    const QVector<Field> fields = scan(text);

    QCOMPARE(fields.size(), 1);
    const Field &field = fields.first();
    QCOMPARE(field.kind, Email);
    // The offset has to index back into the original string, because the UI
    // highlights the field in place.
    QCOMPARE(text.mid(field.start, field.length), field.raw);
    QCOMPARE(field.raw, QStringLiteral("bob@example.com"));
}

QTEST_APPLESS_MAIN(TestFieldParser)
#include "tst_fieldparser.moc"
