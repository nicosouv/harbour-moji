#include "fieldparser.h"

#include <QRegularExpression>
#include <QRegularExpressionMatch>
#include <QRegularExpressionMatchIterator>

#include <algorithm>

namespace {

QString stripSeparators(const QString &input)
{
    QString out = input;
    out.remove(QRegularExpression(QStringLiteral("[\\s\\-\\.]")));
    return out;
}

int valueOfMrzChar(QChar c)
{
    if (c.isDigit()) {
        return c.digitValue();
    }
    if (c == QLatin1Char('<')) {
        return 0;
    }
    if (c >= QLatin1Char('A') && c <= QLatin1Char('Z')) {
        return c.unicode() - 'A' + 10;
    }
    return -1;
}

// Ranges already accounted for by a higher-priority kind, so a 13-digit ISBN is
// not also reported as a card number.
struct Claimed
{
    QVector<QPair<int, int>> ranges;

    bool overlaps(int start, int length) const
    {
        const int end = start + length;
        for (const QPair<int, int> &range : ranges) {
            if (start < range.second && range.first < end) {
                return true;
            }
        }
        return false;
    }

    void claim(int start, int length) { ranges.append(qMakePair(start, start + length)); }
};

} // namespace

namespace FieldParser {

bool ibanValid(const QString &iban)
{
    const QString s = stripSeparators(iban).toUpper();

    // ISO 13616 allows 15 to 34; nothing shorter can carry a country, a checksum
    // and an account.
    if (s.length() < 15 || s.length() > 34) {
        return false;
    }
    if (!s.at(0).isLetter() || !s.at(1).isLetter()
        || !s.at(2).isDigit() || !s.at(3).isDigit()) {
        return false;
    }

    const QString rearranged = s.mid(4) + s.left(4);

    // Folded as we go: the expanded number runs to ~40 digits, well past 64 bits,
    // and mod is distributive over the digit-by-digit construction.
    int remainder = 0;
    for (QChar c : rearranged) {
        if (c.isDigit()) {
            remainder = (remainder * 10 + c.digitValue()) % 97;
        } else if (c.isLetter()) {
            const int value = c.unicode() - 'A' + 10;
            remainder = (remainder * 100 + value) % 97;
        } else {
            return false;
        }
    }

    return remainder == 1;
}

bool luhnValid(const QString &digits)
{
    const QString s = stripSeparators(digits);
    if (s.length() < 2) {
        return false;
    }

    int sum = 0;
    bool doubling = false;
    for (int i = s.length() - 1; i >= 0; --i) {
        if (!s.at(i).isDigit()) {
            return false;
        }
        int d = s.at(i).digitValue();
        if (doubling) {
            d *= 2;
            if (d > 9) {
                d -= 9;
            }
        }
        sum += d;
        doubling = !doubling;
    }

    return sum % 10 == 0;
}

bool isbnValid(const QString &isbn)
{
    const QString s = stripSeparators(isbn).toUpper();

    if (s.length() == 10) {
        int sum = 0;
        for (int i = 0; i < 10; ++i) {
            const QChar c = s.at(i);
            int value;
            if (c.isDigit()) {
                value = c.digitValue();
            } else if (i == 9 && c == QLatin1Char('X')) {
                // Only the check position may be X, and there it means 10.
                value = 10;
            } else {
                return false;
            }
            sum += (i + 1) * value;
        }
        return sum % 11 == 0;
    }

    if (s.length() == 13) {
        int sum = 0;
        for (int i = 0; i < 13; ++i) {
            if (!s.at(i).isDigit()) {
                return false;
            }
            sum += s.at(i).digitValue() * ((i % 2 == 0) ? 1 : 3);
        }
        return sum % 10 == 0;
    }

    return false;
}

int mrzCheckDigit(const QString &input)
{
    static const int weights[3] = { 7, 3, 1 };

    int sum = 0;
    for (int i = 0; i < input.length(); ++i) {
        const int value = valueOfMrzChar(input.at(i));
        if (value < 0) {
            return -1;
        }
        sum += value * weights[i % 3];
    }

    return sum % 10;
}

bool mrzTd3Valid(const QString &line1, const QString &line2)
{
    // TD3 is the passport format: two lines of exactly 44.
    if (line1.length() != 44 || line2.length() != 44) {
        return false;
    }
    if (!line1.startsWith(QLatin1Char('P'))) {
        return false;
    }

    const QString passportNumber = line2.mid(0, 9);
    const QString birthDate = line2.mid(13, 6);
    const QString expiryDate = line2.mid(21, 6);

    // The composite runs over the document number and its check digit, the birth
    // date and its check digit, and everything from the personal number to the
    // last character before the composite digit itself.
    const QString composite = line2.mid(0, 10) + line2.mid(13, 7) + line2.mid(21, 22);

    const int numberCheck = line2.at(9).digitValue();
    const int birthCheck = line2.at(19).digitValue();
    const int expiryCheck = line2.at(27).digitValue();
    const int compositeCheck = line2.at(43).digitValue();

    return mrzCheckDigit(passportNumber) == numberCheck
           && mrzCheckDigit(birthDate) == birthCheck
           && mrzCheckDigit(expiryDate) == expiryCheck
           && mrzCheckDigit(composite) == compositeCheck;
}

QString kindName(Kind kind)
{
    switch (kind) {
    case Iban:       return QStringLiteral("IBAN");
    case CreditCard: return QStringLiteral("Card number");
    case Isbn:       return QStringLiteral("ISBN");
    case MrzLine:    return QStringLiteral("Passport");
    case Email:      return QStringLiteral("Email");
    case Url:        return QStringLiteral("Link");
    case Unknown:    break;
    }
    return QStringLiteral("Unknown");
}

QVector<Field> scan(const QString &text)
{
    QVector<Field> found;
    Claimed claimed;

    const auto addField = [&](Kind kind, const QString &raw, int start,
                              const QString &normalised, bool valid) {
        Field field;
        field.kind = kind;
        field.raw = raw;
        field.normalised = normalised;
        field.start = start;
        field.length = raw.length();
        field.checksumValid = valid;
        found.append(field);
        claimed.claim(start, raw.length());
    };

    // 1. A passport's machine-readable zone: two 44-character lines together.
    //    First because it is the least ambiguous thing on this list, and because
    //    its lines would otherwise be mined for card numbers.
    QRegularExpression mrz(QStringLiteral("([A-Z0-9<]{44})\\s*\\n\\s*([A-Z0-9<]{44})"));
    auto mrzMatches = mrz.globalMatch(text);
    while (mrzMatches.hasNext()) {
        const QRegularExpressionMatch m = mrzMatches.next();
        const QString line1 = m.captured(1);
        const QString line2 = m.captured(2);
        addField(MrzLine, m.captured(0), m.capturedStart(0),
                 line1 + QLatin1Char('\n') + line2, mrzTd3Valid(line1, line2));
    }

    // 2. IBAN. Reported even when the checksum fails: the shape is distinctive
    //    enough that "this looks like an IBAN and the digits are wrong" is the
    //    single most useful thing the app can say about a photographed invoice.
    QRegularExpression iban(QStringLiteral("\\b([A-Z]{2}[0-9]{2}(?:[ ]?[A-Z0-9]){11,30})\\b"));
    auto ibanMatches = iban.globalMatch(text);
    while (ibanMatches.hasNext()) {
        const QRegularExpressionMatch m = ibanMatches.next();
        if (claimed.overlaps(m.capturedStart(1), m.captured(1).length())) {
            continue;
        }
        const QString raw = m.captured(1);
        addField(Iban, raw, m.capturedStart(1),
                 stripSeparators(raw).toUpper(), ibanValid(raw));
    }

    // 3. ISBN, and 4. card numbers. Both are runs of digits and a 13-digit ISBN
    //    has exactly the shape of a short card number, so unlike the IBAN these
    //    are only reported when the checksum agrees - or, for an ISBN, when the
    //    text said "ISBN" out loud. Guessing wrong here would label somebody's
    //    bank card a book.
    QRegularExpression isbn(QStringLiteral(
        "\\b(ISBN(?:-1[03])?:?\\s*)?((?:97[89][- ]?)?(?:[0-9][- ]?){8,11}[0-9Xx])\\b"));
    auto isbnMatches = isbn.globalMatch(text);
    while (isbnMatches.hasNext()) {
        const QRegularExpressionMatch m = isbnMatches.next();
        const QString raw = m.captured(2);
        if (claimed.overlaps(m.capturedStart(2), raw.length())) {
            continue;
        }
        const bool labelled = !m.captured(1).isEmpty();
        const bool valid = isbnValid(raw);
        if (valid || labelled) {
            addField(Isbn, raw, m.capturedStart(2),
                     stripSeparators(raw).toUpper(), valid);
        }
    }

    QRegularExpression card(QStringLiteral("\\b((?:[0-9][ -]?){12,18}[0-9])\\b"));
    auto cardMatches = card.globalMatch(text);
    while (cardMatches.hasNext()) {
        const QRegularExpressionMatch m = cardMatches.next();
        const QString raw = m.captured(1);
        if (claimed.overlaps(m.capturedStart(1), raw.length())) {
            continue;
        }
        const QString digits = stripSeparators(raw);
        if (digits.length() >= 13 && digits.length() <= 19 && luhnValid(digits)) {
            addField(CreditCard, raw, m.capturedStart(1), digits, true);
        }
    }

    // 5. Things with no checksum, reported as valid because there is nothing to
    //    check: they either match the shape or they are not found at all.
    QRegularExpression email(QStringLiteral(
        "\\b[A-Za-z0-9._%+\\-]+@[A-Za-z0-9.\\-]+\\.[A-Za-z]{2,}\\b"));
    auto emailMatches = email.globalMatch(text);
    while (emailMatches.hasNext()) {
        const QRegularExpressionMatch m = emailMatches.next();
        if (claimed.overlaps(m.capturedStart(0), m.captured(0).length())) {
            continue;
        }
        addField(Email, m.captured(0), m.capturedStart(0), m.captured(0), true);
    }

    QRegularExpression url(QStringLiteral("(?:https?://|www\\.)[^\\s]+"));
    auto urlMatches = url.globalMatch(text);
    while (urlMatches.hasNext()) {
        const QRegularExpressionMatch m = urlMatches.next();
        if (claimed.overlaps(m.capturedStart(0), m.captured(0).length())) {
            continue;
        }
        addField(Url, m.captured(0), m.capturedStart(0), m.captured(0), true);
    }

    // Reading order, not detection order: the list is shown next to the image.
    std::sort(found.begin(), found.end(),
              [](const Field &a, const Field &b) { return a.start < b.start; });

    return found;
}

} // namespace FieldParser
